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

#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include "os/mynewt.h"
#include "mcu/cmsis_nvic.h"
#include "mcu/hal_apollo2.h"
#include "hal/hal_timer.h"

#include "am_mcu_apollo.h"

/**
 * Note: Each "BSP timer" is implemented using two MCU timers:
 *
 * 1. Continuous timer - This timer is constantly running.  It provides
 *    absolute time values, and is used for converting between relative and
 *    absolute times.  Its output compare registers are never set.
 *
 * 2. "Once" timer - This timer is only used for generating interrupts at
 *    scheduled times.  It is restarted at 0 for each scheduled event, and only
 *    relative times are used with this timer.
 *
 * As with other HAL timer implementations, event expiry values are stored in
 * absolute tick values.  To set the "once" timer's output compare register,
 * the code uses the continuous timer to determine the current time, and uses
 * the result to calculate the relative offset of the scheduled event.  The
 * relative time then gets written to the "once" timer's output compare
 * register.
 *
 * This scheme introduces some inaccuracy.  Some amount of time invariably
 * passes after the current time is read and before the output compare register
 * is written.  This gap in time causes the timer interrupt to occur later than
 * it should.  This procedure is done in a critical section to minimize error.
 *
 * This somewhat convoluted scheme is required due to hardware limitations.
 * Ideally, each BSP timer would be implemented using a single continuous MCU
 * timer.  However, the MCU only allows a timer to generate a single interrupt
 * while it is running.  To schedule a second event, the timer would need to be
 * stopped, cleared, and started again, which defeats the purpose of a
 * continuous timer.
 */

#define APOLLO2_TIMER_ANY_ENABLED   \
    (MYNEWT_VAL(TIMER_0_SOURCE) || MYNEWT_VAL(TIMER_1_SOURCE))

struct apollo2_timer {
    TAILQ_HEAD(hal_timer_qhead, hal_timer) hal_timer_q;
    struct apollo2_timer_cfg cfg;
    uint32_t freq_hz;       /* Actual frequency. */

    /* Index of continuous timer; measures absolute time. */
    uint8_t cont_timer_idx;

    /* Index of 'once' timer; used for scheduling interrupts. */
    uint8_t once_timer_idx;
};

/**
 * These lookup tables map frequency values to timer configuration settings.
 * They are used for selecting a configuration that is closest to the user's
 * requested frequency.
 *
 * Note: These tables must be in ascending order of frequency.
 */
struct apollo2_timer_freq_entry {
    uint32_t freq;
    uint32_t cfg;
};

static const struct apollo2_timer_freq_entry apollo2_timer_tbl_hfrc[] = {
    { 12000,    AM_HAL_CTIMER_HFRC_12KHZ },
    { 47000,    AM_HAL_CTIMER_HFRC_47KHZ },
    { 187500,   AM_HAL_CTIMER_HFRC_187_5KHZ },
    { 3000000,  AM_HAL_CTIMER_HFRC_3MHZ },
    { 12000000, AM_HAL_CTIMER_HFRC_12MHZ },
    { 0 },
};

static const struct apollo2_timer_freq_entry apollo2_timer_tbl_xt[] = {
    { 256,      AM_HAL_CTIMER_XT_256HZ },
    { 2048,     AM_HAL_CTIMER_XT_2_048KHZ },
    { 16384,    AM_HAL_CTIMER_XT_16_384KHZ },
    { 32768,    AM_HAL_CTIMER_XT_32_768KHZ },
    { 0 },
};

static const struct apollo2_timer_freq_entry apollo2_timer_tbl_lfrc[] = {
    { 1,        AM_HAL_CTIMER_LFRC_1HZ },
    { 32,       AM_HAL_CTIMER_LFRC_32HZ },
    { 512,      AM_HAL_CTIMER_LFRC_512HZ },
    { 1024,     AM_HAL_CTIMER_LFRC_1_16HZ },
    { 0 },
};

#if MYNEWT_VAL(TIMER_0_SOURCE)
static struct apollo2_timer apollo2_timer_0 = {
    .hal_timer_q = TAILQ_HEAD_INITIALIZER(apollo2_timer_0.hal_timer_q),
    .cont_timer_idx = 0,
    .once_timer_idx = 1,
};
#endif
#if MYNEWT_VAL(TIMER_1_SOURCE)
static struct apollo2_timer apollo2_timer_1 = {
    .hal_timer_q = TAILQ_HEAD_INITIALIZER(apollo2_timer_1.hal_timer_q),
    .cont_timer_idx = 2,
    .once_timer_idx = 3,
};
#endif

static struct apollo2_timer *
apollo2_timer_resolve(int timer_num)
{
    switch (timer_num) {
#if MYNEWT_VAL(TIMER_0_SOURCE)
        case 0:     return &apollo2_timer_0;
#endif
#if MYNEWT_VAL(TIMER_1_SOURCE)
        case 1:     return &apollo2_timer_1;
#endif
        default:    return NULL;
    }
}

/**
 * Retrieves the entry from a lookup table whose frequency value most closely
 * matches the one specified.
 */
static const struct apollo2_timer_freq_entry *
apollo2_timer_tbl_find(const struct apollo2_timer_freq_entry *table,
                       uint32_t freq)
{
    const struct apollo2_timer_freq_entry *prev;
    const struct apollo2_timer_freq_entry *cur;
    uint32_t delta1;
    uint32_t delta2;
    int i;

    /* If the requested value is less than all entries in the table, return the
     * smallest one.
     */
    if (table[0].freq >= freq) {
        return &table[0];
    }

    /* Find the first entry with a frequency value that is greater than the one
     * being requested.  Then determine which of it or its predecessor is
     * closer to the specified value.
     */
    for (i = 1; table[i].freq != 0; i++) {
        cur = &table[i];
        if (cur->freq >= freq) {
            prev = cur - 1;
            delta1 = freq - prev->freq;
            delta2 = cur->freq - freq;

            if (delta1 <= delta2) {
                return prev;
            } else {
                return cur;
            }
        }
    }

    /* Requested value is greater than all entries in the table; return the
     * largest.
     */
    return table + i - 1;
}

/**
 * Calculates the best SDK configuration value for the specified timer.  The
 * calculated value is called an "SDK configuration value" because it gets
 * passed to the Apollo2 SDK timer configuration function.  Flags specific to
 * the continuous or "once" timer are not included in the result; these must be
 * ORed in, depending on the MCU timer being configured.
 */
static int
apollo2_timer_sdk_cfg(const struct apollo2_timer_cfg *cfg, uint32_t freq_hz,
                      uint32_t *out_actual_hz, uint32_t *out_cfg)
{
    const struct apollo2_timer_freq_entry *entry;

    switch (cfg->source) {
    case APOLLO2_TIMER_SOURCE_HFRC:
        entry = apollo2_timer_tbl_find(apollo2_timer_tbl_hfrc, freq_hz);
        *out_actual_hz = entry->freq;
        *out_cfg = entry->cfg;
        return 0;

    case APOLLO2_TIMER_SOURCE_XT:
        entry = apollo2_timer_tbl_find(apollo2_timer_tbl_xt, freq_hz);
        *out_actual_hz = entry->freq;
        *out_cfg = entry->cfg;
        return 0;

    case APOLLO2_TIMER_SOURCE_LFRC:
        entry = apollo2_timer_tbl_find(apollo2_timer_tbl_lfrc, freq_hz);
        *out_actual_hz = entry->freq;
        *out_cfg = entry->cfg;
        return 0;

    case APOLLO2_TIMER_SOURCE_RTC:
        *out_actual_hz = 100;
        *out_cfg = AM_HAL_CTIMER_RTC_100HZ;
        return 0;

    case APOLLO2_TIMER_SOURCE_HCLK:
        *out_actual_hz = 48000000;
        *out_cfg = AM_HAL_CTIMER_HCLK;
        return 0;

    default:
        return SYS_EINVAL;
    }
}

/**
 * Calculates the value to write to the specified timer's ISR configuration
 * register.
 */ 
static int
apollo2_timer_isr_cfg(const struct apollo2_timer *bsp_timer,
                      uint32_t *out_isr_cfg)
{
    switch (bsp_timer->once_timer_idx) {
#if MYNEWT_VAL(TIMER_0_SOURCE)
    case 1:
        *out_isr_cfg = AM_HAL_CTIMER_INT_TIMERA1C0;
        return 0;
#endif
#if MYNEWT_VAL(TIMER_1_SOURCE)
    case 3:
        *out_isr_cfg = AM_HAL_CTIMER_INT_TIMERA3C0;
        return 0;
#endif
    default:
        return SYS_EINVAL;
    }
}

/**
 * Retrieves the current time from the specified timer.
 */
static uint32_t
apollo2_timer_cur_ticks(const struct apollo2_timer *bsp_timer)
{
    return am_hal_ctimer_read(bsp_timer->cont_timer_idx, AM_HAL_CTIMER_BOTH);
}

/**
 * Configures a BSP timer to generate an interrupt at the speficied relative
 * time.
 */
static void
apollo2_timer_set_ocmp(const struct apollo2_timer *bsp_timer,
                       uint32_t ticks_from_now)
{
    uint32_t isr_cfg;
    int rc;

    /* Calculate the ISR flags for the "once" timer. */
    rc = apollo2_timer_isr_cfg(bsp_timer, &isr_cfg);
    assert(rc == 0);

    /* Clear any pending interrupt for this timer. */
    am_hal_ctimer_int_clear(isr_cfg);

    /* Stop and clear the "once" timer. */
    am_hal_ctimer_stop(bsp_timer->once_timer_idx, AM_HAL_CTIMER_BOTH);
    am_hal_ctimer_clear(bsp_timer->once_timer_idx, AM_HAL_CTIMER_BOTH);

    /* Schedule an interrupt at the requested relative time. */
    am_hal_ctimer_period_set(bsp_timer->once_timer_idx, AM_HAL_CTIMER_BOTH,
                             ticks_from_now, 0);

    /* Enable interrupts for this timer, in case they haven't been enabled
     * yet.
     */
    am_hal_ctimer_int_enable(isr_cfg);

    /* Restart the timer. */
    am_hal_ctimer_start(bsp_timer->once_timer_idx, AM_HAL_CTIMER_BOTH);
}

/**
 * Configures a BSP timer to generate an interrupt at the speficied absolute
 * time.
 */
static void
apollo2_timer_set_ocmp_at(const struct apollo2_timer *bsp_timer, uint32_t at)
{
    uint32_t isr_cfg;
    uint32_t now;
    int32_t ticks_from_now;
    int rc;

    now = apollo2_timer_cur_ticks(bsp_timer);
    ticks_from_now = at - now;
    if (ticks_from_now <= 0) {
        /* Event already occurred. */
        rc = apollo2_timer_isr_cfg(bsp_timer, &isr_cfg);
        assert(rc == 0);
        am_hal_ctimer_int_set(isr_cfg);
    } else {
        apollo2_timer_set_ocmp(bsp_timer, ticks_from_now);
    }
}

/**
 * Unsets a scheduled interrupt for the specified BSP timer.
 */
static void
apollo2_timer_clear_ocmp(const struct apollo2_timer *bsp_timer)
{
    uint32_t isr_cfg;
    int rc;

    rc = apollo2_timer_isr_cfg(bsp_timer, &isr_cfg);
    assert(rc == 0);

    am_hal_ctimer_int_disable(isr_cfg);
}

#if APOLLO2_TIMER_ANY_ENABLED
/**
 * Executes callbacks for all expired timers in a BSP timer's queue.  This
 * function is called when a timer interrupt is handled.
 */
static void
apollo2_timer_chk_queue(struct apollo2_timer *bsp_timer)
{
    struct hal_timer *timer;
    uint32_t ticks;
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);

    /* Remove and process each expired timer in the sorted queue. */
    while ((timer = TAILQ_FIRST(&bsp_timer->hal_timer_q)) != NULL) {
        ticks = apollo2_timer_cur_ticks(bsp_timer);
        if ((int32_t)(ticks - timer->expiry) >= 0) {
            TAILQ_REMOVE(&bsp_timer->hal_timer_q, timer, link);
            timer->link.tqe_prev = NULL;
            timer->cb_func(timer->cb_arg);
        } else {
            break;
        }
    }

    /* If any timers remain, schedule an interrupt for the timer that expires
     * next.
     */
    if (timer != NULL) {
        apollo2_timer_set_ocmp_at(bsp_timer, timer->expiry);
    } else {
        apollo2_timer_clear_ocmp(bsp_timer);
    }

    OS_EXIT_CRITICAL(sr);
}
#endif

/**
 * Handles a ctimer interrupt.
 */
static void
apollo2_timer_isr(void)
{
    uint32_t status;

    /* Read the ctimer status to determine which timers generated the
     * interrupt.
     */
    status = am_hal_ctimer_int_status_get(true);
    am_hal_ctimer_int_clear(status);

    /* Service the appropriate timers. */
#if MYNEWT_VAL(TIMER_0_SOURCE)
    if (status & (AM_HAL_CTIMER_INT_TIMERA1C0 | AM_HAL_CTIMER_INT_TIMERA1C1)) {
        apollo2_timer_chk_queue(&apollo2_timer_0);
    }
#endif
#if MYNEWT_VAL(TIMER_1_SOURCE)
    if (status & (AM_HAL_CTIMER_INT_TIMERA3C0 | AM_HAL_CTIMER_INT_TIMERA3C1)) {
        apollo2_timer_chk_queue(&apollo2_timer_1);
    }
#endif
}

/**
 * hal timer init
 *
 * Initialize platform specific timer items
 *
 * @param timer_num     Timer number to initialize
 * @param cfg           Pointer to platform specific configuration
 *
 * @return int          0: success; error code otherwise
 */
int
hal_timer_init(int timer_num, void *vcfg)
{
#if !APOLLO2_TIMER_ANY_ENABLED
    return SYS_EINVAL;
#endif

    static int nvic_configured;

    const struct apollo2_timer_cfg *bsp_cfg;
    struct apollo2_timer *bsp_timer;

    bsp_timer = apollo2_timer_resolve(timer_num);
    if (bsp_timer == NULL) {
        return SYS_EINVAL;
    }

    if (!nvic_configured) {
        nvic_configured = 1;

        NVIC_SetVector(CTIMER_IRQn, (uint32_t)apollo2_timer_isr);
        NVIC_SetPriority(CTIMER_IRQn, (1 << __NVIC_PRIO_BITS) - 1);
        NVIC_ClearPendingIRQ(CTIMER_IRQn);
        NVIC_EnableIRQ(CTIMER_IRQn);
    }

    bsp_cfg = vcfg;
    bsp_timer->cfg = *bsp_cfg;

    return 0;
}

/**
 * hal timer config
 *
 * Configure a timer to run at the desired frequency. This starts the timer.
 *
 * @param timer_num
 * @param freq_hz
 *
 * @return int
 */
int
hal_timer_config(int timer_num, uint32_t freq_hz)
{
#if !APOLLO2_TIMER_ANY_ENABLED
    return SYS_EINVAL;
#endif

    struct apollo2_timer *bsp_timer;
    uint32_t cont_cfg;
    uint32_t once_cfg;
    uint32_t sdk_cfg;
    int rc;

    bsp_timer = apollo2_timer_resolve(timer_num);
    if (bsp_timer == NULL) {
        return SYS_EINVAL;
    }

    rc = apollo2_timer_sdk_cfg(&bsp_timer->cfg, freq_hz, &bsp_timer->freq_hz,
                               &sdk_cfg);
    if (rc != 0) {
        return rc;
    }

    /* Configure the continuous timer. */
    cont_cfg = sdk_cfg | AM_HAL_CTIMER_FN_CONTINUOUS;
    am_hal_ctimer_clear(bsp_timer->cont_timer_idx, AM_HAL_CTIMER_BOTH);
    am_hal_ctimer_config_single(bsp_timer->cont_timer_idx, AM_HAL_CTIMER_BOTH,
                                cont_cfg);

    /* Configure the "once" timer. */
    once_cfg = sdk_cfg | AM_HAL_CTIMER_FN_ONCE | AM_HAL_CTIMER_INT_ENABLE;
    am_hal_ctimer_clear(bsp_timer->once_timer_idx, AM_HAL_CTIMER_BOTH);
    am_hal_ctimer_config_single(bsp_timer->once_timer_idx, AM_HAL_CTIMER_BOTH,
                                once_cfg);

    /* Start the continuous timer. */
    am_hal_ctimer_start(bsp_timer->cont_timer_idx, AM_HAL_CTIMER_BOTH);

    return 0;
}

/**
 * hal timer deinit
 *
 * De-initialize a HW timer.
 *
 * @param timer_num
 *
 * @return int
 */
int
hal_timer_deinit(int timer_num)
{
#if !APOLLO2_TIMER_ANY_ENABLED
    return SYS_EINVAL;
#endif

    struct apollo2_timer *bsp_timer;

    bsp_timer = apollo2_timer_resolve(timer_num);
    if (bsp_timer == NULL) {
        return SYS_EINVAL;
    }

    if (bsp_timer->freq_hz == 0) {
        /* Timer not enabled. */
        return SYS_ENODEV;
    }

    am_hal_ctimer_stop(bsp_timer->cont_timer_idx, AM_HAL_CTIMER_BOTH);
    am_hal_ctimer_stop(bsp_timer->once_timer_idx, AM_HAL_CTIMER_BOTH);
    bsp_timer->freq_hz = 0;

    return 0;
}

/**
 * hal timer get resolution
 *
 * Get the resolution of the timer. This is the timer period, in nanoseconds
 *
 * @param timer_num
 *
 * @return uint32_t
 */
uint32_t
hal_timer_get_resolution(int timer_num)
{
#if !APOLLO2_TIMER_ANY_ENABLED
    return 0;
#endif

    const struct apollo2_timer *bsp_timer;

    bsp_timer = apollo2_timer_resolve(timer_num);
    if (bsp_timer == NULL) {
        return 0;
    }

    if (bsp_timer->freq_hz == 0) {
        return 0;
    }

    return 1000000000 / bsp_timer->freq_hz;
}

/**
 * Reads the absolute time from the specified continuous timer.
 *
 * @return uint32_t The timer counter register.
 */
uint32_t
hal_timer_read(int timer_num)
{
#if !APOLLO2_TIMER_ANY_ENABLED
    return SYS_EINVAL;
#endif

    const struct apollo2_timer *bsp_timer;

    bsp_timer = apollo2_timer_resolve(timer_num);
    if (bsp_timer == NULL) {
        return 0;
    }

    if (bsp_timer->freq_hz == 0) {
        /* Timer not enabled. */
        return 0;
    }

    return apollo2_timer_cur_ticks(bsp_timer);
}

/**
 * Blocking delay for n ticks
 *
 * @return int 0 on success; error code otherwise.
 */
int
hal_timer_delay(int timer_num, uint32_t ticks)
{
#if !APOLLO2_TIMER_ANY_ENABLED
    return SYS_EINVAL;
#endif

    const struct apollo2_timer *bsp_timer;
    uint32_t until;

    bsp_timer = apollo2_timer_resolve(timer_num);
    if (bsp_timer == NULL) {
        return SYS_EINVAL;
    }

    until = apollo2_timer_cur_ticks(bsp_timer) + ticks;
    while ((int32_t)(apollo2_timer_cur_ticks(bsp_timer) - until) <= 0) { }

    return 0;
}

/**
 * Initialize the HAL timer structure with the callback and the callback
 * argument.
 *
 * @param cb_func
 *
 * @return int
 */
int
hal_timer_set_cb(int timer_num, struct hal_timer *timer, hal_timer_cb cb_func,
                 void *arg)
{
#if !APOLLO2_TIMER_ANY_ENABLED
    return SYS_EINVAL;
#endif

    struct apollo2_timer *bsp_timer;

    bsp_timer = apollo2_timer_resolve(timer_num);
    if (bsp_timer == NULL) {
        return SYS_EINVAL;
    }

    timer->cb_func = cb_func;
    timer->cb_arg = arg;
    timer->bsp_timer = bsp_timer;
    timer->link.tqe_prev = NULL;

    return 0;
}

/**
 * Start a timer. Timer fires 'ticks' ticks from now.
 *
 * @param timer
 * @param ticks
 *
 * @return int
 */
int
hal_timer_start(struct hal_timer *timer, uint32_t ticks)
{
#if !APOLLO2_TIMER_ANY_ENABLED
    return SYS_EINVAL;
#endif

    const struct apollo2_timer *bsp_timer;
    uint32_t exp;
    uint32_t cur;
    os_sr_t sr;
    int rc;

    bsp_timer = timer->bsp_timer;

    /* Start a critical section before reading the current time.  Preventing
     * this task from being interrupted minimizes inaccuracy when converting
     * from relative to absolute time.
     */
    OS_ENTER_CRITICAL(sr);

    cur = apollo2_timer_cur_ticks(bsp_timer);
    exp = ticks + cur;

    rc = hal_timer_start_at(timer, exp);

    OS_EXIT_CRITICAL(sr);

    return rc;
}

/**
 * hal_timer_start_at()
 *
 * Start a timer. Timer fires at tick 'tick'.
 *
 * @param timer
 * @param tick
 *
 * @return int
 */
int
hal_timer_start_at(struct hal_timer *timer, uint32_t tick)
{
#if !APOLLO2_TIMER_ANY_ENABLED
    return SYS_EINVAL;
#endif

    struct apollo2_timer *bsp_timer;
    struct hal_timer *cur;
    os_sr_t sr;

    bsp_timer = timer->bsp_timer;
    timer->expiry = tick;

    OS_ENTER_CRITICAL(sr);

    if (TAILQ_EMPTY(&bsp_timer->hal_timer_q)) {
        TAILQ_INSERT_HEAD(&bsp_timer->hal_timer_q, timer, link);
    } else {
        TAILQ_FOREACH(cur, &bsp_timer->hal_timer_q, link) {
            if ((int32_t)(timer->expiry - cur->expiry) < 0) {
                TAILQ_INSERT_BEFORE(cur, timer, link);
                break;
            }
        }
        if (cur == NULL) {
            TAILQ_INSERT_TAIL(&bsp_timer->hal_timer_q, timer, link);
        }
    }

    if (timer == TAILQ_FIRST(&bsp_timer->hal_timer_q)) {
        apollo2_timer_set_ocmp_at(bsp_timer, tick);
    }

    OS_EXIT_CRITICAL(sr);

    return 0;
}

/**
 * hal_timer_stop()
 *
 * Cancels the timer.
 *
 * @param timer
 *
 * @return int
 */
int
hal_timer_stop(struct hal_timer *timer)
{
#if !APOLLO2_TIMER_ANY_ENABLED
    return SYS_EINVAL;
#endif

    struct apollo2_timer *bsp_timer;
    int reset_ocmp;
    os_sr_t sr;

    /* If timer's prev pointer is null, the timer hasn't been started. */
    if (timer->link.tqe_prev == NULL) {
        return 0;
    }

    bsp_timer = timer->bsp_timer;

    OS_ENTER_CRITICAL(sr);

    if (timer == TAILQ_FIRST(&bsp_timer->hal_timer_q)) {
        /* If first on queue, we will need to reset OCMP */
        reset_ocmp = 1;
    }

    TAILQ_REMOVE(&bsp_timer->hal_timer_q, timer, link);
    timer->link.tqe_prev = NULL;

    if (reset_ocmp) {
        timer = TAILQ_FIRST(&bsp_timer->hal_timer_q);
        if (timer != NULL) {
            apollo2_timer_set_ocmp_at(bsp_timer, timer->expiry);
        } else {
            apollo2_timer_clear_ocmp(bsp_timer);
        }
    }

    OS_EXIT_CRITICAL(sr);

    return 0;
}
