/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "os/os_trace_api.h"
#include "mcu/da1469x_dma.h"
#include "mcu/cmsis_nvic.h"
#include "defs/error.h"

#define MCU_DMA_CHAN2CIDX(_chan)    ((_chan) - ((struct da1469x_dma_regs *)DMA))
#define MCU_DMA_CIDX2CHAN(_cidx)    (&((struct da1469x_dma_regs *)DMA)[(_cidx)])
#define MCU_DMA_CHAN_MAX            (8)

#define MCU_DMA_SET_MUX(_cidx, _periph)             \
    do {                                            \
        DMA->DMA_REQ_MUX_REG =                      \
            (DMA->DMA_REQ_MUX_REG &                 \
             ~(0xf << ((_cidx) * 4))) |             \
            ((_periph) << ((_cidx) * 4));           \
    } while (0)
#define MCU_DMA_GET_MUX(_cidx)                      \
    (DMA->DMA_REQ_MUX_REG >> ((_cidx) * 4) & 0xf)

struct da1469x_dma_interrupt_cfg {
    da1469x_dma_interrupt_cb cb;
    void *arg;
};

static uint8_t g_da1469x_dma_acquired;
static uint8_t g_da1469x_dma_isr_set;
static struct da1469x_dma_interrupt_cfg g_da1469x_dma_isr_cfg[MCU_DMA_CHAN_MAX];

static inline int
find_free_single(void)
{
    int cidx;

    for (cidx = 0; cidx < MCU_DMA_CHAN_MAX; cidx++) {
        if ((g_da1469x_dma_acquired & (1 << cidx)) == 0) {
            return cidx;
        }
    }

    return -1;
}

static inline int
find_free_pair(void)
{
    int cidx;

    for (cidx = 0; cidx < MCU_DMA_CHAN_MAX; cidx += 2) {
        if ((g_da1469x_dma_acquired & (3 << cidx)) == 0) {
            return cidx;
        }
    }

    return -1;
}

static void
dma_handler(void)
{
    int cidx;

    os_trace_isr_enter();

    for (cidx = 0; cidx < MCU_DMA_CHAN_MAX; cidx++) {
        if ((DMA->DMA_INT_STATUS_REG & (1 << cidx)) == 0) {
            continue;
        }

        DMA->DMA_CLEAR_INT_REG = 1 << cidx;

        if (g_da1469x_dma_isr_cfg[cidx].cb) {
            g_da1469x_dma_isr_cfg[cidx].cb(g_da1469x_dma_isr_cfg[cidx].arg);
        }
    }

    os_trace_isr_exit();
}

void
da1469x_dma_init(void)
{
    NVIC_DisableIRQ(DMA_IRQn);
    NVIC_SetVector(DMA_IRQn, (uint32_t)dma_handler);
}

struct da1469x_dma_regs *
da1469x_dma_acquire_single(int cidx)
{
    struct da1469x_dma_regs *chan;

    assert(cidx < MCU_DMA_CHAN_MAX);

    if (cidx < 0) {
        cidx = find_free_single();
        if (cidx < 0) {
            return NULL;
        }
    } else if (g_da1469x_dma_acquired & (1 << cidx)) {
        return NULL;
    }

    g_da1469x_dma_acquired |= 1 << cidx;

    chan = MCU_DMA_CIDX2CHAN(cidx);

    MCU_DMA_SET_MUX(cidx, MCU_DMA_PERIPH_NONE);

    chan->DMA_CTRL_REG &= ~DMA_DMA0_CTRL_REG_DREQ_MODE_Msk;

    return chan;
}

int
da1469x_dma_acquire_periph(int cidx, uint8_t periph,
                           struct da1469x_dma_regs *chans[2])
{
    assert(cidx < MCU_DMA_CHAN_MAX);

    if (cidx < 0) {
        cidx = find_free_pair();
        if (cidx < 0) {
            return SYS_ENOENT;
        }
    } else {
        cidx &= 0xfe;
        if (g_da1469x_dma_acquired & (3 << cidx)) {
            return SYS_EBUSY;
        }
    }

    g_da1469x_dma_acquired |= 3 << cidx;

    chans[0] = MCU_DMA_CIDX2CHAN(cidx);
    chans[1] = MCU_DMA_CIDX2CHAN(cidx + 1);

    MCU_DMA_SET_MUX(cidx, periph);

    chans[0]->DMA_CTRL_REG |= DMA_DMA0_CTRL_REG_DREQ_MODE_Msk;
    chans[1]->DMA_CTRL_REG |= DMA_DMA0_CTRL_REG_DREQ_MODE_Msk;

    return 0;
}

int
da1469x_dma_release_channel(struct da1469x_dma_regs *chan)
{
    int cidx = MCU_DMA_CHAN2CIDX(chan);

    assert(cidx >= 0 && cidx < MCU_DMA_CHAN_MAX);

    /*
     * If corresponding pair for this channel is configured for triggering from
     * peripheral, we'll use lower of channel index.
     */
    if (MCU_DMA_GET_MUX(cidx) != MCU_DMA_PERIPH_NONE) {
        cidx &= 0xfe;
        chan = MCU_DMA_CIDX2CHAN(cidx);

        chan[0].DMA_CTRL_REG &= ~DMA_DMA0_CTRL_REG_DMA_ON_Msk;
        chan[1].DMA_CTRL_REG &= ~DMA_DMA0_CTRL_REG_DMA_ON_Msk;

        MCU_DMA_SET_MUX(cidx, MCU_DMA_PERIPH_NONE);

        g_da1469x_dma_acquired &= ~(3 << cidx);

        g_da1469x_dma_isr_set &= ~(3 << cidx);
        DMA->DMA_CLEAR_INT_REG = 3 << cidx;

        memset(&g_da1469x_dma_isr_cfg[cidx], 0,
               sizeof(struct da1469x_dma_interrupt_cfg) * 2);
    } else {
        chan->DMA_CTRL_REG &= ~DMA_DMA0_CTRL_REG_DMA_ON_Msk;
        g_da1469x_dma_acquired &= ~(1 << cidx);

        g_da1469x_dma_isr_set &= ~(1 << cidx);
        DMA->DMA_CLEAR_INT_REG = 1 << cidx;
        memset(&g_da1469x_dma_isr_cfg[cidx], 0,
               sizeof(struct da1469x_dma_interrupt_cfg));
    }

    DMA->DMA_INT_MASK_REG = g_da1469x_dma_isr_set;
    if (!g_da1469x_dma_isr_set) {
        NVIC_DisableIRQ(DMA_IRQn);
    }

    return 0;
}

int
da1469x_dma_configure(struct da1469x_dma_regs *chan,
                      const struct da1469x_dma_config *cfg,
                      da1469x_dma_interrupt_cb isr_cb, void *isr_arg)
{
    uint32_t regval;
    int cidx = MCU_DMA_CHAN2CIDX(chan);

    regval = chan->DMA_CTRL_REG & ~(DMA_DMA0_CTRL_REG_AINC_Msk |
                                    DMA_DMA0_CTRL_REG_AINC_Msk |
                                    DMA_DMA0_CTRL_REG_BINC_Msk |
                                    DMA_DMA0_CTRL_REG_DMA_PRIO_Msk |
                                    DMA_DMA0_CTRL_REG_BW_Msk |
                                    DMA_DMA0_CTRL_REG_BURST_MODE_Msk);
    regval |= DMA_DMA0_CTRL_REG_BUS_ERROR_DETECT_Msk;
    regval |= cfg->src_inc << DMA_DMA0_CTRL_REG_AINC_Pos;
    regval |= cfg->dst_inc << DMA_DMA0_CTRL_REG_BINC_Pos;
    regval |= cfg->priority << DMA_DMA0_CTRL_REG_DMA_PRIO_Pos;
    regval |= cfg->bus_width << DMA_DMA0_CTRL_REG_BW_Pos;
    regval |= cfg->burst_mode << DMA_DMA0_CTRL_REG_BURST_MODE_Pos;
    chan->DMA_CTRL_REG = regval;

    g_da1469x_dma_isr_cfg[cidx].cb = isr_cb;
    g_da1469x_dma_isr_cfg[cidx].arg = isr_arg;

    if (isr_cb) {
        g_da1469x_dma_isr_set |= 1 << cidx;
    } else {
        g_da1469x_dma_isr_set &= ~(1 << cidx);
    }

    DMA->DMA_INT_MASK_REG = g_da1469x_dma_isr_set;

    if (g_da1469x_dma_isr_set) {
        NVIC_EnableIRQ(DMA_IRQn);
    }

    return 0;
}
