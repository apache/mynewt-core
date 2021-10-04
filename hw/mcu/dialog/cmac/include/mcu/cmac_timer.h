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

#ifndef __MCU_CMAC_TIMER_H_
#define __MCU_CMAC_TIMER_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Equals to LLT value after HAL timer wrapped around,
 * i.e. os_cputime(0xffffffff "+1")
 */
#define CMAC_TIMER_LLT_AT_HAL_WRAP_AROUND_VAL   (0x1e84800000)

#define CMAC_TIMER_HAL_GT(_t1, _t2)             ((int32_t)((_t1) - (_t2)) > 0)
#define CMAC_TIMER_HAL_LT(_t1, _t2)             ((int32_t)((_t1) - (_t2)) < 0)

struct cmac_timer_ctrl {
    uint32_t llt_last_hi_val;
    uint64_t llt_corr;

    uint32_t hal_last_val;
    uint64_t hal_to_llt_corr;
};

typedef void (cmac_timer_int_func_t)(void);

extern volatile uint32_t cm_ll_int_stat_reg;
extern struct cmac_timer_ctrl g_cmac_timer_ctrl;

void cmac_timer_init(void);
void cmac_timer_slp_enable(uint32_t ticks);
void cmac_timer_slp_disable(uint32_t exp_ticks);
bool cmac_timer_slp_update(void);
bool cmac_timer_slp_is_ready(void);
#if MYNEWT_VAL(MCU_SLP_TIMER_32K_ONLY)
static inline uint32_t
cmac_timer_slp_tick_us(void)
{
    return 31;
}
#else
uint32_t cmac_timer_slp_tick_us(void);
#endif
void cmac_timer_int_hal_timer_register(cmac_timer_int_func_t func);
void cmac_timer_int_os_tick_register(cmac_timer_int_func_t func);
void cmac_timer_int_os_tick_clear(void);

uint32_t cmac_timer_next_at(void);
uint32_t cmac_timer_usecs_to_lp_ticks(uint32_t usecs);

uint32_t cmac_timer_get_hal_os_tick(void);

static inline uint32_t
cmac_timer_read_lo(void)
{
    return CMAC->CM_LL_TIMER1_9_0_REG;
}

static inline uint32_t
cmac_timer_read_hi(void)
{
    return CMAC->CM_LL_TIMER1_36_10_REG;
}

/* Reads lsb 32-bit value of LL Timer */
static inline uint32_t
cmac_timer_read32(void)
{
    uint32_t hi;
    uint32_t lo;

    do {
        hi = cmac_timer_read_hi();
        lo = cmac_timer_read_lo();
    } while (hi != cmac_timer_read_hi());

    return (hi << 10) | lo;
}

/* Reads full 37-bit value of LL Timer */
static inline uint64_t
cmac_timer_read64(void)
{
    uint32_t hi;
    uint32_t lo;
    uint32_t llt_corr;

    do {
        hi = cmac_timer_read_hi();
        lo = cmac_timer_read_lo();
    } while (hi != cmac_timer_read_hi());

    __disable_irq();

    /*
     * We extend LLT value manually to full 64-bit timer, this gives us plenty
     * of resolution so we don't really need to care about wrap around. Note
     * that we keep track of hi-word correction only (bits 63:37) so there is
     * no need for 64-bit calculations on correction value - final shift by 32
     * bits is optimized by a compiler to a simple 32-bit operation on hi-word.
     */

    if (hi < g_cmac_timer_ctrl.llt_last_hi_val) {
        g_cmac_timer_ctrl.llt_corr += 1 << 5;
    }
    g_cmac_timer_ctrl.llt_last_hi_val = hi;
    llt_corr = g_cmac_timer_ctrl.llt_corr;

    __enable_irq();

    return ((uint64_t)llt_corr << 32) | ((uint64_t)hi << 10) | lo;
}

static inline void
cmac_timer_trigger_hal(void)
{
    cm_ll_int_stat_reg = CMAC_CM_LL_INT_STAT_REG_LL_TIMER1_EQ_X_SEL_Msk;
    NVIC_SetPendingIRQ(LL_TIMER2LLC_IRQn);
}

/* Write comparator value for hal_timer callback */
static inline void
cmac_timer_write_eq_hal_timer(uint64_t val)
{
    CMAC->CM_LL_TIMER1_EQ_X_HI_REG = val >> 10;
    CMAC->CM_LL_TIMER1_EQ_X_LO_REG = val;
    CMAC->CM_LL_INT_MSK_SET_REG = CMAC_CM_LL_INT_MSK_SET_REG_LL_TIMER1_EQ_X_SEL_Msk;

    /*
     * Need to check if we set comparator in the past (e.g. when trying to set
     * it at "now" and force interrupt. Note that we cannot mark particular
     * comparator as triggered so we just use local variable as a "shadow"
     * register.
     */
    if ((int64_t)(val - cmac_timer_read64()) <= 0) {
        cm_ll_int_stat_reg = CMAC_CM_LL_INT_STAT_REG_LL_TIMER1_EQ_X_SEL_Msk;
        NVIC_SetPendingIRQ(LL_TIMER2LLC_IRQn);
    }
}

/* Disable comparator value for hal_timer callback */
static inline void
cmac_timer_disable_eq_hal_timer(void)
{
    CMAC->CM_LL_INT_MSK_CLR_REG = CMAC_CM_LL_INT_MSK_CLR_REG_LL_TIMER1_EQ_X_SEL_Msk;
}

/* Write comparator value for hal_os_tick callback */
static inline void
cmac_timer_write_eq_hal_os_tick(uint64_t val)
{
    uint32_t val_hi;

    val_hi = (val >> 10) & 0x7ffffff;

    CMAC->CM_LL_TIMER1_36_10_EQ_Y_REG = val_hi;

    if ((int32_t)(val_hi - cmac_timer_read_hi()) <= 0) {
        cm_ll_int_stat_reg = CMAC_CM_LL_INT_STAT_REG_LL_TIMER1_36_10_EQ_Y_SEL_Msk;
        NVIC_SetPendingIRQ(LL_TIMER2LLC_IRQn);
    }
}

/* Convert ll_timer value to hal_timer value */
static inline uint64_t
cmac_timer_convert_llt2hal(uint64_t val)
{
    return (val << 9) / 15625;
}

/* Convert ll_timer value to os_tick value */
static inline uint32_t
cmac_timer_convert_llt2tck(uint64_t val)
{
    return (val << 1) / 15625;
}

/* Convert hal_timer value to ll_timer value */
static inline uint64_t
cmac_timer_convert_hal2llt(uint32_t val)
{
    uint64_t llt;
    uint32_t v1, v2;
    int64_t dt;

    /*
     * Use known HAL timer value and LLT value pair as a base, then apply
     * correction based on delta between known HAL timer value and value being
     * converted.
     */

    __disable_irq();

    v1 = g_cmac_timer_ctrl.hal_last_val;
    v2 = val;
    llt = g_cmac_timer_ctrl.hal_to_llt_corr;

    __enable_irq();

    dt = (int32_t)(v2 - v1);
    llt += (dt * 15625) / 512;

    return llt;
}

/* Convert os_tick value to ll_timer value */
static inline uint64_t
cmac_timer_convert_tck2llt(uint32_t val)
{
    return ((uint64_t)val * 15625) >> 1;
}

#ifdef __cplusplus
}
#endif

#endif /* __MCU_CMAC_TIMER_H_ */
