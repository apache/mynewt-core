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
#include "mcu/mcu.h"
#include "mcu/da1469x_snc.h"

int
snc_sw_init(void)
{
    /* First, just put it S/W control. */
    SNC->SNC_CTRL_REG = SNC_SNC_CTRL_REG_SNC_SW_CTRL_Msk;

    /* XXX: is this the proper order? Should this call simply stop it? What
       if PDC has control? */
    /* SNC better be stopped! */
    if ((SNC->SNC_STATUS_REG & SNC_SNC_STATUS_REG_SNC_IS_STOPPED_Msk) == 0) {
        return -1;
    }

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
snc_sw_start(void)
{
    /* XXX: Should we check if already running and return error if not? */
    SNC->SNC_CTRL_REG |= SNC_SNC_CTRL_REG_SNC_EN_Msk;
    return 0;
}

int
snc_sw_stop(void)
{
    /* XXX: Should we check if in S/W control and return error if not? */
    SNC->SNC_CTRL_REG &= ~SNC_SNC_CTRL_REG_SNC_EN_Msk;
    return 0;
}

int
snc_program_is_done(void)
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

int
snc_irq_config(uint8_t mask)
{
    int rc;
    uint32_t irqs;

    rc = 0;
    if (SNC->SNC_STATUS_REG & SNC_SNC_CTRL_REG_SNC_IRQ_EN_Msk) {
        snc_irq_clear();
    }

    if (mask != SNC_IRQ_MASK_NONE) {
        if (mask > (SNC_IRQ_MASK_HOST | SNC_IRQ_MASK_PDC)) {
            rc = -1;
        } else {
            irqs = 0;
            if (mask & SNC_IRQ_MASK_HOST) {
                irqs |= (1 << 6);
            }
            if (mask & SNC_IRQ_MASK_PDC) {
                irqs |= (1 << 7);
            }
            SNC->SNC_CTRL_REG |= irqs;
        }
    } else {
        /* No interrupts from SNC */
        SNC->SNC_CTRL_REG &= ~SNC_SNC_CTRL_REG_SNC_IRQ_CONFIG_Msk;
    }
    return rc;
}

int
snc_config(void *prog_addr, int clk_div)
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

    CRG_COM->CLK_COM_REG |= (clk_div << CRG_COM_CLK_COM_REG_SNC_DIV_Pos);

    return 0;
}
