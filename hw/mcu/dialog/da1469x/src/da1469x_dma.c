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
#include "mcu/da1469x_pd.h"
#include "mcu/cmsis_nvic.h"
#include "defs/error.h"

#define MCU_DMA_CHAN2CIDX(_chan)    ((_chan) - ((struct da1469x_dma_regs *)DMA))
#define MCU_DMA_CIDX2CHAN(_cidx)    (&((struct da1469x_dma_regs *)DMA)[(_cidx)])
#ifndef MCU_DMA_CHAN_MAX
#define MCU_DMA_CHAN_MAX            (8)
#endif

#define DA1469X_DMA_MUX_SHIFT(_cidx) (((_cidx) >> 1) * 4)

/*
 * Mux Bits 0:3, 4:7, 8:11, 12:15 correspond to selection in
 * channels 0,1  2,3, 4,5 & 6,7. Use the shift macro above to
 * determine the mask.
 */
#define MCU_DMA_GET_MUX(_cidx)                                                       \
    ((DMA->DMA_REQ_MUX_REG >> DA1469X_DMA_MUX_SHIFT(_cidx)) & 0xf)

#define MCU_DMA_SET_MUX(_cidx, _periph)                                              \
    DMA->DMA_REQ_MUX_REG =                                                           \
        ((DMA->DMA_REQ_MUX_REG & ~(0xf << DA1469X_DMA_MUX_SHIFT(_cidx)))             \
         | ((_periph) & 0xf) << DA1469X_DMA_MUX_SHIFT(_cidx));                       \


struct da1469x_dma_interrupt_cfg {
    da1469x_dma_interrupt_cb cb;
    void *arg;
};

#if (MCU_DMA_CHAN_MAX) > 8
static uint16_t g_da1469x_dma_acquired;
static uint16_t g_da1469x_dma_isr_set;
#else
static uint8_t g_da1469x_dma_acquired;
static uint8_t g_da1469x_dma_isr_set;
#endif

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
    NVIC_SetPriority(DMA_IRQn, MYNEWT_VAL(MCU_DMA_IRQ_PRIO));
}

struct da1469x_dma_regs *
da1469x_dma_acquire_single(int cidx)
{
    struct da1469x_dma_regs *chan = NULL;
    int sr;

    assert(cidx < MCU_DMA_CHAN_MAX);

    OS_ENTER_CRITICAL(sr);
    if (cidx < 0) {
        cidx = find_free_single();
        if (cidx < 0) {
            goto end_single;
        }
    } else if (g_da1469x_dma_acquired & (1 << cidx)) {
        goto end_single;
    }

    if (!g_da1469x_dma_acquired) {
        da1469x_pd_acquire(MCU_PD_DOMAIN_SYS);
    }

    g_da1469x_dma_acquired |= 1 << cidx;

    chan = MCU_DMA_CIDX2CHAN(cidx);

    /*
     * DMA_REQ_MUX_REG applies only to channels < 8
     */
    if (cidx < 8) {
        MCU_DMA_SET_MUX(cidx, MCU_DMA_PERIPH_NONE);
    }

    chan->DMA_CTRL_REG &= ~DMA_DMA0_CTRL_REG_DREQ_MODE_Msk;

end_single:
    OS_EXIT_CRITICAL(sr);
    return chan;
}

int
da1469x_dma_acquire_periph(int cidx, uint8_t periph,
                           struct da1469x_dma_regs *chans[2])
{
    int sr;
    int rc = 0;
    assert(cidx < MCU_DMA_CHAN_MAX && periph < MCU_DMA_PERIPH_NONE);
    OS_ENTER_CRITICAL(sr);

    if (cidx < 0) {
        cidx = find_free_pair();
        if (cidx < 0) {
            rc = SYS_ENOENT;
            goto out;
        }
    } else {
        cidx &= 0xfe;
        if (g_da1469x_dma_acquired & (3 << cidx)) {
            rc = SYS_EBUSY;
            goto out;
        }
    }

    if (!g_da1469x_dma_acquired) {
        da1469x_pd_acquire(MCU_PD_DOMAIN_SYS);
    }

    g_da1469x_dma_acquired |= 3 << cidx;

    chans[0] = MCU_DMA_CIDX2CHAN(cidx);
    chans[1] = MCU_DMA_CIDX2CHAN(cidx + 1);

    MCU_DMA_SET_MUX(cidx, periph);

    chans[0]->DMA_CTRL_REG |= DMA_DMA0_CTRL_REG_DREQ_MODE_Msk;
    chans[1]->DMA_CTRL_REG |= DMA_DMA0_CTRL_REG_DREQ_MODE_Msk;
out:
    OS_EXIT_CRITICAL(sr);
    return rc;
}

int
da1469x_dma_release_channel(struct da1469x_dma_regs *chan)
{
    int cidx = MCU_DMA_CHAN2CIDX(chan);
    int sr;

    assert(cidx >= 0 && cidx < MCU_DMA_CHAN_MAX);

    OS_ENTER_CRITICAL(sr);
    /*
     * If corresponding pair for this channel is configured for triggering from
     * peripheral, we'll use lower of channel index.
     *
     * Only channels 0-7 can use pairs for peripherals.
     */
    if (cidx < 8 && MCU_DMA_GET_MUX(cidx) < MCU_DMA_PERIPH_NONE) {
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

    if (!g_da1469x_dma_acquired) {
        da1469x_pd_release(MCU_PD_DOMAIN_SYS);
    }

    OS_EXIT_CRITICAL(sr);
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

int
da1469x_dma_write_peripheral(struct da1469x_dma_regs *chan, const void *mem, uint16_t size)
{
    uint32_t dma_mem;

    if (chan == NULL || mem == NULL || size == 0 || chan->DMA_B_START_REG == 0) {
        return SYS_EINVAL;
    }
    /*
     * DMA can't access QSPI flash in cached region 0x16000000-0x18000000.
     * It can access same area uncached which is 0x36000000-0x38000000.
     */
    if (MCU_MEM_QSPIF_M_RANGE_ADDRESS(mem)) {
        dma_mem = (uint32_t)mem + 0x20000000;
    } else {
        dma_mem = (uint32_t)mem;
    }
    chan->DMA_A_START_REG = dma_mem;
    chan->DMA_INT_REG = size - 1;
    chan->DMA_LEN_REG = size - 1;
    chan->DMA_CTRL_REG |= DMA_DMA0_CTRL_REG_DMA_ON_Msk;

    return SYS_EOK;
}

int
da1469x_dma_read_peripheral(struct da1469x_dma_regs *chan, void *mem, uint16_t size)
{
    if (chan == NULL || mem == NULL || size == 0 || chan->DMA_A_START_REG == 0) {
        return SYS_EINVAL;
    }
    chan->DMA_B_START_REG = (uint32_t)mem;
    chan->DMA_INT_REG = size - 1;
    chan->DMA_LEN_REG = size - 1;
    chan->DMA_CTRL_REG |= DMA_DMA0_CTRL_REG_DMA_ON_Msk;

    return SYS_EOK;
}
