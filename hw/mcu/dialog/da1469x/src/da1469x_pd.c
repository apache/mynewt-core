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
#include "defs/error.h"
#include "mcu/da1469x_hal.h"
#include "mcu/da1469x_pd.h"
#include "os/util.h"

struct da1469x_pd_desc {
    uint8_t pmu_sleep_bit;
    uint8_t stat_down_bit; /* up is +1 */
};

static const struct da1469x_pd_desc g_da1469x_pd_desc[] = {
    [MCU_PD_DOMAIN_PER] = { CRG_TOP_PMU_CTRL_REG_PERIPH_SLEEP_Pos,
                            CRG_TOP_SYS_STAT_REG_PER_IS_DOWN_Pos },
    [MCU_PD_DOMAIN_RAD] = { CRG_TOP_PMU_CTRL_REG_RADIO_SLEEP_Pos,
                            CRG_TOP_SYS_STAT_REG_RAD_IS_DOWN_Pos},
    [MCU_PD_DOMAIN_TIM] = { CRG_TOP_PMU_CTRL_REG_TIM_SLEEP_Pos,
                            CRG_TOP_SYS_STAT_REG_TIM_IS_DOWN_Pos },
    [MCU_PD_DOMAIN_COM] = { CRG_TOP_PMU_CTRL_REG_COM_SLEEP_Pos,
                            CRG_TOP_SYS_STAT_REG_COM_IS_DOWN_Pos},
};

static uint8_t g_da1469x_pd_refcnt[ ARRAY_SIZE(g_da1469x_pd_desc) ];

int
da1469x_pd_acquire(uint8_t pd)
{
    uint8_t *refcnt;
    uint32_t primask;
    uint32_t bitmask;
    int ret = 0;

    assert(pd < ARRAY_SIZE(g_da1469x_pd_desc));
    refcnt = &g_da1469x_pd_refcnt[pd];

    __HAL_DISABLE_INTERRUPTS(primask);

    assert(*refcnt < UINT8_MAX);
    if ((*refcnt)++ == 0) {
        bitmask = 1 << g_da1469x_pd_desc[pd].pmu_sleep_bit;
        CRG_TOP->PMU_CTRL_REG &= ~bitmask;

        bitmask = 1 << (g_da1469x_pd_desc[pd].stat_down_bit + 1);
        while ((CRG_TOP->SYS_STAT_REG & bitmask) == 0);

        ret = 1;
    }

    __HAL_ENABLE_INTERRUPTS(primask);

    return ret;
}

int
da1469x_pd_release(uint8_t pd)
{
    uint8_t *refcnt;
    uint32_t primask;
    uint32_t bitmask;
    int ret = 0;

    assert(pd < MCU_PD_DOMAIN_COUNT);
    refcnt = &g_da1469x_pd_refcnt[pd];

    __HAL_DISABLE_INTERRUPTS(primask);

    assert(*refcnt > 0);
    if (--(*refcnt) == 0) {
        bitmask = 1 << g_da1469x_pd_desc[pd].pmu_sleep_bit;
        CRG_TOP->PMU_CTRL_REG |= bitmask;

        bitmask = 1 << g_da1469x_pd_desc[pd].stat_down_bit;
        while ((CRG_TOP->SYS_STAT_REG & bitmask) == 0);

        ret = 1;
    }

    __HAL_ENABLE_INTERRUPTS(primask);

    return ret;
}

int
da1469x_pd_release_nowait(uint8_t pd)
{
    uint8_t *refcnt;
    uint32_t primask;
    uint32_t bitmask;
    int ret = 0;

    assert(pd < MCU_PD_DOMAIN_COUNT);
    refcnt = &g_da1469x_pd_refcnt[pd];

    __HAL_DISABLE_INTERRUPTS(primask);

    assert(*refcnt > 0);
    if (--(*refcnt) == 0) {
        bitmask = 1 << g_da1469x_pd_desc[pd].pmu_sleep_bit;
        CRG_TOP->PMU_CTRL_REG |= bitmask;

        ret = 1;
    }

    __HAL_ENABLE_INTERRUPTS(primask);

    return ret;
}
