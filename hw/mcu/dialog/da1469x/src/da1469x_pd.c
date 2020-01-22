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

struct da1469x_pd_desc {
    uint8_t pmu_sleep_bit;
    uint8_t stat_down_bit; /* up is +1 */
};

struct da1469x_pd_trimv {
    uint8_t count;
    uint32_t *words;
};

static const struct da1469x_pd_desc g_da1469x_pd_desc[] = {
    [MCU_PD_DOMAIN_SYS] = { CRG_TOP_PMU_CTRL_REG_SYS_SLEEP_Pos,
                            CRG_TOP_SYS_STAT_REG_SYS_IS_DOWN_Pos },
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
static struct da1469x_pd_trimv g_da1469x_pd_trimv[ ARRAY_SIZE(g_da1469x_pd_desc) ];

static void
da1469x_pd_load_trimv(uint8_t pd, uint8_t group)
{
    struct da1469x_pd_trimv *tv;

    assert(pd < ARRAY_SIZE(g_da1469x_pd_desc));

    tv = &g_da1469x_pd_trimv[pd];
    tv->count = da1469x_trimv_group_num_words_get(group);
    if (tv->count == 0) {
        return;
    }

    tv->words = calloc(tv->count, 4);
    if (!tv->words) {
        tv->count = 0;
        assert(0);
        return;
    }

    tv->count = da1469x_trimv_group_read(group, tv->words, tv->count);
    tv->count /= 2;
}

static void
da1469x_pd_apply_trimv(uint8_t pd)
{
    struct da1469x_pd_trimv *tv;
    volatile uint32_t *reg;
    uint32_t val;
    int idx;

    assert(pd < ARRAY_SIZE(g_da1469x_pd_desc));

    tv = &g_da1469x_pd_trimv[pd];
    if (tv->count == 0) {
        return;
    }

    for (idx = 0; idx < tv->count; idx++) {
        reg = (void *)tv->words[idx * 2];
        val = tv->words[idx * 2 + 1];
        *reg = val;
    }
}

static inline uint32_t
get_reg32(uint32_t addr)
{
    volatile uint32_t *reg = (volatile uint32_t *)addr;

    return *reg;
}

static inline void
set_reg32(uint32_t addr, uint32_t val)
{
    volatile uint32_t *reg = (volatile uint32_t *)addr;

    *reg = val;
}

static inline void
set_reg32_mask(uint32_t addr, uint32_t mask, uint32_t val)
{
    volatile uint32_t *reg = (volatile uint32_t *)addr;

    *reg = (*reg & (~mask)) | (val & mask);
}

static void
da1469x_pd_apply_preferred(uint8_t pd)
{
    switch (pd) {
    case MCU_PD_DOMAIN_SYS:
        set_reg32_mask(0x50040400, 0x00000c00, 0x003f6a78);
        set_reg32_mask(0x50040454, 0x000003ff, 0x00000002);
        break;
    case MCU_PD_DOMAIN_TIM:
        set_reg32_mask(0x50010000, 0x3ff00000, 0x000afd70);
        set_reg32_mask(0x50010010, 0x000000c0, 0x00000562);
        set_reg32_mask(0x50010030, 0x43c38002, 0x4801e6b6);
        set_reg32_mask(0x50010034, 0x007fff00, 0x7500a1a4);
        set_reg32_mask(0x50010038, 0x00000fff, 0x001e45c4);
        set_reg32_mask(0x5001003c, 0x40000000, 0x40096255);
        set_reg32_mask(0x50010040, 0x00c00000, 0x00c00000);
        set_reg32_mask(0x50010018, 0x000000ff, 0x00000180);
        break;
    }
}

static void
apply_preferred_pd_aon(void)
{
    if (get_reg32(0x500000f8) == 0x00008800) {
        set_reg32(0x500000f8, 0x00007700);
    }
    set_reg32_mask(0x50000050, 0x00001000, 0x00001020);
    set_reg32(0x500000a4, 0x000000ca);
    set_reg32_mask(0x50000064, 0x0003ffff, 0x041e6ef4);
}

int
da1469x_pd_init(void)
{
    /*
     * Apply now for always-on domain which, as name suggests, is always on so
     * need to do this only once.
     */
    apply_preferred_pd_aon();

    da1469x_pd_load_trimv(MCU_PD_DOMAIN_SYS, 1);
    da1469x_pd_load_trimv(MCU_PD_DOMAIN_COM, 2);
    da1469x_pd_load_trimv(MCU_PD_DOMAIN_TIM, 4);
    da1469x_pd_load_trimv(MCU_PD_DOMAIN_PER, 5);

    return 0;
}

static int
da1469x_pd_acquire_internal(uint8_t pd, bool load)
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
