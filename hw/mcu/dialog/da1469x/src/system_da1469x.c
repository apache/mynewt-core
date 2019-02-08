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
#include "DA1469xAB.h"

#define PMU_ALL_SLEEP_MASK      (CRG_TOP_PMU_CTRL_REG_TIM_SLEEP_Msk | \
                                 CRG_TOP_PMU_CTRL_REG_PERIPH_SLEEP_Msk | \
                                 CRG_TOP_PMU_CTRL_REG_COM_SLEEP_Msk | \
                                 CRG_TOP_PMU_CTRL_REG_RADIO_SLEEP_Msk)
#define SYS_ALL_IS_UP_MASK      (CRG_TOP_SYS_STAT_REG_TIM_IS_UP_Msk | \
                                 CRG_TOP_SYS_STAT_REG_PER_IS_UP_Msk | \
                                 CRG_TOP_SYS_STAT_REG_COM_IS_UP_Msk | \
                                 CRG_TOP_SYS_STAT_REG_RAD_IS_UP_Msk)

void SystemInit(void)
{
    assert(CHIP_VERSION->CHIP_ID1_REG == '2');
    assert(CHIP_VERSION->CHIP_ID2_REG == '5');
    assert(CHIP_VERSION->CHIP_ID3_REG == '2');
    assert(CHIP_VERSION->CHIP_ID4_REG == '2');
    assert(CHIP_VERSION->CHIP_REVISION_REG == 'A');
    assert(CHIP_VERSION->CHIP_TEST1_REG == 'B');

    /* Enable FPU when using hard-float */
#if (__FPU_USED == 1)
    SCB->CPACR |= (3UL << 20) | (3UL << 22);
    __DSB();
    __ISB();
#endif

    /* Enable Radio LDO */
    CRG_TOP->POWER_CTRL_REG |= CRG_TOP_POWER_CTRL_REG_LDO_RADIO_ENABLE_Msk;

    /* Enable PD_TIM, PD_PER, PD_COM and PD_RAD */
    CRG_TOP->PMU_CTRL_REG &= ~PMU_ALL_SLEEP_MASK;
    while ((CRG_TOP->SYS_STAT_REG & SYS_ALL_IS_UP_MASK) != SYS_ALL_IS_UP_MASK);

    /* Keep CMAC in reset, we don't need it now */
    CRG_TOP->CLK_RADIO_REG = (0 << CRG_TOP_CLK_RADIO_REG_RFCU_ENABLE_Pos) |
                             (1 << CRG_TOP_CLK_RADIO_REG_CMAC_SYNCH_RESET_Pos) |
                             (0 << CRG_TOP_CLK_RADIO_REG_CMAC_CLK_SEL_Pos) |
                             (0 << CRG_TOP_CLK_RADIO_REG_CMAC_CLK_ENABLE_Pos) |
                             (0 << CRG_TOP_CLK_RADIO_REG_CMAC_DIV_Pos);

    NVIC_DisableIRQ(PDC_IRQn);
    NVIC_ClearPendingIRQ(PDC_IRQn);
}
