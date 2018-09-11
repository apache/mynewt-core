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

#include <mcu/cortex_m4.h>
#include "hal/hal_nvreg.h"
#include "nrf.h"

/* There are two GPREGRET registers on the NRF52 */
#define HAL_NVREG_MAX (2)

/* GPREGRET registers only save the 8 lsbits */
#define HAL_NVREG_WIDTH_BYTES (1)

static volatile uint32_t *regs[HAL_NVREG_MAX] = {
    &NRF_POWER->GPREGRET,
    &NRF_POWER->GPREGRET2
};

void
hal_nvreg_write(unsigned int reg, uint32_t val)
{
    if(reg < HAL_NVREG_MAX) {
        *regs[reg] = val;
    }
}

uint32_t
hal_nvreg_read(unsigned int reg)
{
    uint32_t val = 0;

    if(reg < HAL_NVREG_MAX) {
        val = *regs[reg];
    }

    return val;
}

unsigned int hal_nvreg_get_num_regs(void)
{
    return HAL_NVREG_MAX;
}

unsigned int hal_nvreg_get_reg_width(void)
{
    return HAL_NVREG_WIDTH_BYTES;
}
