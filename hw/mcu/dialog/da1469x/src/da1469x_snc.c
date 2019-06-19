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
#include "os/os_trace_api.h"
#include "mcu/da1469x_snc.h"
#include "mcu/da1469x_pd.h"

/* IRQ callback for M33 and argument */
snc_isr_cb_t g_snc_isr_cb_func;
void *g_snc_isr_arg;

static void
da1469x_snc_irq_handler(void)
{
    os_trace_isr_enter();
    da1469x_snc_irq_clear();
    if (g_snc_isr_cb_func) {
        g_snc_isr_cb_func(g_snc_isr_arg);
    }
    os_trace_isr_exit();
}

int
da1469x_snc_sw_init(void)
{
    /* SNC better be stopped! */
    if ((SNC->SNC_STATUS_REG & SNC_SNC_STATUS_REG_SNC_IS_STOPPED_Msk) == 0) {
        return -1;
    }

    /* First, just put it S/W control. */
    SNC->SNC_CTRL_REG = SNC_SNC_CTRL_REG_SNC_SW_CTRL_Msk;


    /* We will be using the COM power domain so acquire it here */
    da1469x_pd_acquire(MCU_PD_DOMAIN_COM);

    /* Reset the SNC (keep in SW ctrl as well) */
    SNC->SNC_CTRL_REG = SNC_SNC_CTRL_REG_SNC_SW_CTRL_Msk |
                        SNC_SNC_CTRL_REG_SNC_RESET_Msk;

    /*
     * Program the following in control register
     *  SNC_SW_CTRL: Puts SNC in software control (PDC does not use SNC).
     *  IRQ_ACK: set just in case to clear any interrupts.
     *  BRANCH_LOOP_INIT: set to clear loop counter.
     *  BUS_ERROR_DETECT: set to enable bus error detection.
     */
    SNC->SNC_CTRL_REG = SNC_SNC_CTRL_REG_SNC_SW_CTRL_Msk |
                        SNC_SNC_CTRL_REG_SNC_BRANCH_LOOP_INIT_Msk |
                        SNC_SNC_CTRL_REG_BUS_ERROR_DETECT_EN_Msk |
                        SNC_SNC_CTRL_REG_SNC_IRQ_ACK_Msk;
    return 0;
}

int
da1469x_snc_sw_deinit(void)
{
    /* SNC better be in SW control! */
    if ((SNC->SNC_CTRL_REG & SNC_SNC_CTRL_REG_SNC_SW_CTRL_Msk) == 0) {
        return -1;
    }

    /* SNC better be stopped! */
    if ((SNC->SNC_STATUS_REG & SNC_SNC_STATUS_REG_SNC_IS_STOPPED_Msk) == 0) {
        return -1;
    }

    /* Take out of SW control */
    SNC->SNC_CTRL_REG &= ~SNC_SNC_CTRL_REG_SNC_SW_CTRL_Msk;

    /* Release COM power domain */
    da1469x_pd_release(MCU_PD_DOMAIN_COM);

    return 0;
}

int
da1469x_snc_sw_start(void)
{
    /* XXX: Should we check if already running and return error if not? */
    SNC->SNC_CTRL_REG |= SNC_SNC_CTRL_REG_SNC_EN_Msk;
    return 0;
}

int
da1469x_snc_sw_stop(void)
{
    /* XXX: Should we check if in S/W control and return error if not? */
    SNC->SNC_CTRL_REG &= ~SNC_SNC_CTRL_REG_SNC_EN_Msk;
    return 0;
}

int
da1469x_snc_program_is_done(void)
{
    int rc;
    uint32_t status;

    status = SNC->SNC_STATUS_REG & SNC_SNC_STATUS_REG_SNC_DONE_STATUS_Msk;
    if (status) {
        rc = 1;
    } else {
        rc = 0;
    }
    return rc;
}

uint8_t
da1469x_snc_error_status(void)
{
    uint8_t err;
    uint32_t status;

    err = 0;
    status = SNC->SNC_STATUS_REG;
    if (status & SNC_SNC_STATUS_REG_BUS_ERROR_STATUS_Msk) {
        err |= SNC_BUS_ERROR;
    }
    if (status & SNC_SNC_STATUS_REG_HARD_FAULT_STATUS_Msk) {
        err |= SNC_HARD_FAULT_ERROR;
    }

    return err;
}

int
da1469x_snc_irq_config(uint8_t mask, snc_isr_cb_t isr_cb, void *arg)
{
    int rc;
    uint32_t irqs;

    NVIC_DisableIRQ(SNC_IRQn);
    NVIC_SetVector(SNC_IRQn, (uint32_t)da1469x_snc_irq_handler);

   /* Clear the bits first... */
   SNC->SNC_CTRL_REG &= ~SNC_SNC_CTRL_REG_SNC_IRQ_CONFIG_Msk;

    rc = 0;
    if (SNC->SNC_STATUS_REG & SNC_SNC_CTRL_REG_SNC_IRQ_EN_Msk) {
        da1469x_snc_irq_clear();
    }

    if (mask != SNC_IRQ_MASK_NONE) {
        if (mask > (SNC_IRQ_MASK_HOST | SNC_IRQ_MASK_PDC)) {
            rc = -1;
        } else {
            irqs = 0;
            if (mask & SNC_IRQ_MASK_HOST) {
                irqs |= (1 << 6);
                g_snc_isr_arg = arg;
                g_snc_isr_cb_func = isr_cb;
                NVIC_EnableIRQ(SNC_IRQn);
            }
            if (mask & SNC_IRQ_MASK_PDC) {
                irqs |= (1 << 7);
            }
            SNC->SNC_CTRL_REG |= irqs;
        }
    }

    return rc;
}

int
da1469x_snc_config(void *prog_addr, int clk_div)
{
    uint32_t offset;

    /* Do some basic checks to make sure it is OK */
    offset = (uint32_t)prog_addr;
    if ((offset & 0x3) || (offset < MCU_MEM_SYSRAM_START_ADDRESS)) {
        return -1;
    }

    /*
     * Program the SNC base address register. This is where the device
     * will execute code.
     *
     * XXX: can probably just write the entire address without mask
     * but just in case mask it.
     */
    MEMCTRL->SNC_BASE_REG = offset &
        MEMCTRL_SNC_BASE_REG_SNC_BASE_ADDRESS_Msk;

    /* Only two bits for clock divider */
    if (clk_div > 3) {
        return -1;
    }

    CRG_COM->CLK_COM_REG &= ~(clk_div << CRG_COM_CLK_COM_REG_SNC_DIV_Pos);
    CRG_COM->CLK_COM_REG |= (clk_div << CRG_COM_CLK_COM_REG_SNC_DIV_Pos);

    return 0;
}
