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
#include <stdlib.h>
#include "defs/error.h"
#include "mcu/da1469x_hal.h"
#include "mcu/da1469x_pd.h"
#include "mcu/da1469x_trimv.h"
#include "os/util.h"

/* This shall be defined for each MCU separately */
extern void da1469x_pd_apply_preferred(uint8_t pd);

#define PD_COUNT    (ARRAY_SIZE(g_da1469x_pd_desc))

struct da1469x_pd_desc {
    uint8_t pmu_sleep_bit;
    uint8_t stat_down_bit; /* up is +1 */
};

struct da1469x_pd_data {
    uint8_t refcnt;
    uint8_t trimv_count;
    uint32_t *trimv_words;
};

/* Only include controllable power domains here */
static const struct da1469x_pd_desc g_da1469x_pd_desc[] = {
    [MCU_PD_DOMAIN_SYS] = { CRG_TOP_PMU_CTRL_REG_SYS_SLEEP_Pos,
                            CRG_TOP_SYS_STAT_REG_SYS_IS_DOWN_Pos },
    [MCU_PD_DOMAIN_PER] = { CRG_TOP_PMU_CTRL_REG_PERIPH_SLEEP_Pos,
                            CRG_TOP_SYS_STAT_REG_PER_IS_DOWN_Pos },
    [MCU_PD_DOMAIN_TIM] = { CRG_TOP_PMU_CTRL_REG_TIM_SLEEP_Pos,
                            CRG_TOP_SYS_STAT_REG_TIM_IS_DOWN_Pos },
    [MCU_PD_DOMAIN_COM] = { CRG_TOP_PMU_CTRL_REG_COM_SLEEP_Pos,
                            CRG_TOP_SYS_STAT_REG_COM_IS_DOWN_Pos},
};

static struct da1469x_pd_data g_da1469x_pd_data[PD_COUNT];

static void
da1469x_pd_load_trimv(uint8_t pd, uint8_t group)
{
    struct da1469x_pd_data *pdd;

    assert(pd < PD_COUNT);

    pdd = &g_da1469x_pd_data[pd];
    pdd->trimv_count = da1469x_trimv_group_num_words_get(group);
    if (pdd->trimv_count == 0) {
        return;
    }

    pdd->trimv_words = calloc(pdd->trimv_count, 4);
    if (!pdd->trimv_words) {
        pdd->trimv_count = 0;
        assert(0);
        return;
    }

    pdd->trimv_count = da1469x_trimv_group_read(group, pdd->trimv_words,
                                                pdd->trimv_count);
    pdd->trimv_count /= 2;
}

static void
da1469x_pd_apply_trimv(uint8_t pd)
{
    struct da1469x_pd_data *pdd;
    volatile uint32_t *reg;
    uint32_t val;
    int idx;

    assert(pd < PD_COUNT);

    pdd = &g_da1469x_pd_data[pd];
    if (pdd->trimv_count == 0) {
        return;
    }

    for (idx = 0; idx < pdd->trimv_count; idx++) {
        reg = (uint32_t *) pdd->trimv_words[idx * 2];
        val = pdd->trimv_words[idx * 2 + 1];
        *reg = val;
    }
}

int
da1469x_pd_init(void)
{
    /*
     * Apply now for always-on domain which, as name suggests, is always on so
     * need to do this only once.
     */
    da1469x_pd_apply_preferred(MCU_PD_DOMAIN_AON);

    da1469x_pd_load_trimv(MCU_PD_DOMAIN_SYS, 1);
    da1469x_pd_load_trimv(MCU_PD_DOMAIN_COM, 2);
    da1469x_pd_load_trimv(MCU_PD_DOMAIN_TIM, 4);
    da1469x_pd_load_trimv(MCU_PD_DOMAIN_PER, 5);

    return 0;
}

static int
da1469x_pd_acquire_internal(uint8_t pd, bool load)
{
    struct da1469x_pd_data *pdd;
    uint32_t primask;
    uint32_t bitmask;
    int ret = 0;

    assert(pd < PD_COUNT);

    pdd = &g_da1469x_pd_data[pd];

    __HAL_DISABLE_INTERRUPTS(primask);

    assert(pdd->refcnt < UINT8_MAX);

    if (pdd->refcnt++ == 0) {
        bitmask = 1 << g_da1469x_pd_desc[pd].pmu_sleep_bit;
        CRG_TOP->PMU_CTRL_REG &= ~bitmask;

        bitmask = 1 << (g_da1469x_pd_desc[pd].stat_down_bit + 1);
        while ((CRG_TOP->SYS_STAT_REG & bitmask) == 0);

        if (load) {
            da1469x_pd_apply_trimv(pd);
            da1469x_pd_apply_preferred(pd);
        }

        ret = 1;
    }

    __HAL_ENABLE_INTERRUPTS(primask);

    return ret;
}

int
da1469x_pd_acquire(uint8_t pd)
{
    return da1469x_pd_acquire_internal(pd, true);
}

int
da1469x_pd_acquire_noconf(uint8_t pd)
{
    return da1469x_pd_acquire_internal(pd, false);
}

static int
da1469x_pd_release_internal(uint8_t pd, bool wait)
{
    struct da1469x_pd_data *pdd;
    uint32_t primask;
    uint32_t bitmask;
    int ret = 0;

    assert(pd < PD_COUNT);

    pdd = &g_da1469x_pd_data[pd];

    __HAL_DISABLE_INTERRUPTS(primask);

    assert(pdd->refcnt > 0);

    if (--pdd->refcnt == 0) {
        bitmask = 1 << g_da1469x_pd_desc[pd].pmu_sleep_bit;
        CRG_TOP->PMU_CTRL_REG |= bitmask;

        if (wait) {
            bitmask = 1 << g_da1469x_pd_desc[pd].stat_down_bit;
            while ((CRG_TOP->SYS_STAT_REG & bitmask) == 0);
        }

        ret = 1;
    }

    __HAL_ENABLE_INTERRUPTS(primask);

    return ret;
}

int
da1469x_pd_release(uint8_t pd)
{
    return da1469x_pd_release_internal(pd, true);
}

int
da1469x_pd_release_nowait(uint8_t pd)
{
    return da1469x_pd_release_internal(pd, false);
}
