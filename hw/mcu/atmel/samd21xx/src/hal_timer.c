/**
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
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include "syscfg/syscfg.h"
#include "mcu/cmsis_nvic.h"
#include "hal/hal_timer.h"
#include "src/sam0/drivers/tc/tc.h"
#include "src/sam0/utils/status_codes.h"
#include "src/common/utils/interrupt/interrupt_sam_nvic.h"
#include "mcu/samd21_hal.h"

/* IRQ prototype */
typedef void (*hal_timer_irq_handler_t)(void);

/* Number of timers for HAL */
#define SAMD21_HAL_TIMER_MAX    (3)

/* Internal timer data structure */
struct samd21_hal_timer {
    uint8_t tmr_enabled;
    uint8_t tmr_irq_num;
    uint8_t tmr_srcclk;
    uint8_t tmr_initialized;
    uint32_t tmr_cntr;
    uint32_t timer_isrs;
    uint32_t tmr_freq;
    TAILQ_HEAD(hal_timer_qhead, hal_timer) hal_timer_q;
    struct tc_module tc_mod;
    enum gclk_generator tmr_clkgen;
};

#if MYNEWT_VAL(TIMER_0)
struct samd21_hal_timer samd21_hal_timer0;
#endif
#if MYNEWT_VAL(TIMER_1)
struct samd21_hal_timer samd21_hal_timer1;
#endif
#if MYNEWT_VAL(TIMER_2)
struct samd21_hal_timer samd21_hal_timer2;
#endif

static const struct samd21_hal_timer *samd21_hal_timers[SAMD21_HAL_TIMER_MAX] = {
#if MYNEWT_VAL(TIMER_0)
    &samd21_hal_timer0,
#else
    NULL,
#endif
#if MYNEWT_VAL(TIMER_1)
    &samd21_hal_timer1,
#else
    NULL,
#endif
#if MYNEWT_VAL(TIMER_2)
    &samd21_hal_timer2,
#else
    NULL,
#endif
};

/* Resolve timer number into timer structure */
#define SAMD21_HAL_TIMER_RESOLVE(__n, __v)       \
    if ((__n) >= SAMD21_HAL_TIMER_MAX) {         \
        rc = EINVAL;                            \
        goto err;                               \
    }                                           \
    (__v) = (struct samd21_hal_timer *) samd21_hal_timers[(__n)];            \
    if ((__v) == NULL) {                        \
        rc = EINVAL;                            \
        goto err;                               \
    }

/**
 * samd21 timer set ocmp
 *
 * Set the OCMP used by the timer to the desired expiration tick
 *
 * NOTE: Must be called with interrupts disabled.
 *
 * @param timer Pointer to timer.
 */
static void
samd21_timer_set_ocmp(struct samd21_hal_timer *bsptimer, uint32_t expiry)
{
    Tc *hwtimer;
    uint16_t expiry16;
    uint32_t temp;
    int32_t delta_t;

    hwtimer = bsptimer->tc_mod.hw;

    /* Disable ocmp interrupt and set new value */
    hwtimer->COUNT16.INTENCLR.reg = TC_INTENCLR_MC0;

    temp = expiry & 0xffff0000;
    delta_t = (int32_t)(temp - bsptimer->tmr_cntr);
    if (delta_t < 0) {
        goto set_ocmp_late;
    } else if (delta_t == 0) {
        /* Set ocmp and check if missed it */
        expiry16 = (uint16_t)expiry;

        /* Set output compare register to timer expiration */
        tc_set_compare_value(&bsptimer->tc_mod, TC_COMPARE_CAPTURE_CHANNEL_0,
                             expiry16);

        /* Clear interrupt flag */
        hwtimer->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;

        /* Enable the output compare interrupt */
        hwtimer->COUNT16.INTENSET.reg = TC_INTENSET_MC0;

        /* Force interrupt to occur as we may have missed it */
        if (tc_get_count_value(&bsptimer->tc_mod) >= expiry16) {
            goto set_ocmp_late;
        }
    } else {
        /* Nothing to do; wait for overflow interrupt to set ocmp */
    }
    return;

set_ocmp_late:
    NVIC_SetPendingIRQ(bsptimer->tmr_irq_num);
}

/* Disable output compare used for timer */
static void
samd21_timer_disable_ocmp(Tc *hwtimer)
{
    hwtimer->COUNT16.INTENCLR.reg = TC_INTENCLR_MC0;
}

static uint32_t
hal_timer_read_bsptimer(struct samd21_hal_timer *bsptimer)
{
    uint16_t low;
    uint32_t tcntr;
    Tc *hwtimer;

    hwtimer = bsptimer->tc_mod.hw;
    cpu_irq_enter_critical();
    tcntr = bsptimer->tmr_cntr;
    low = tc_get_count_value(&bsptimer->tc_mod);
    if (hwtimer->COUNT16.INTFLAG.reg & TC_INTFLAG_OVF) {
        tcntr += 65536;
        bsptimer->tmr_cntr = tcntr;
        low = tc_get_count_value(&bsptimer->tc_mod);
        hwtimer->COUNT16.INTFLAG.reg = TC_INTFLAG_OVF;
        NVIC_SetPendingIRQ(bsptimer->tmr_irq_num);
    }
    tcntr |= low;
    cpu_irq_leave_critical();

    return tcntr;
}


#if (MYNEWT_VAL(TIMER_0) || MYNEWT_VAL(TIMER_1) || MYNEWT_VAL(TIMER_2))
/**
 * hal timer chk queue
 *
 * @param bsptimer
 */
static void
hal_timer_chk_queue(struct samd21_hal_timer *bsptimer)
{
    uint32_t tcntr;
    struct hal_timer *timer;

    /* disable interrupts */
    cpu_irq_enter_critical();

    while ((timer = TAILQ_FIRST(&bsptimer->hal_timer_q)) != NULL) {
        tcntr = hal_timer_read_bsptimer(bsptimer);
        if ((int32_t)(tcntr - timer->expiry) >= 0) {
            TAILQ_REMOVE(&bsptimer->hal_timer_q, timer, link);
            timer->link.tqe_prev = NULL;
            timer->cb_func(timer->cb_arg);
        } else {
            break;
        }
    }

    /* Any timers left on queue? If so, we need to set OCMP */
    timer = TAILQ_FIRST(&bsptimer->hal_timer_q);
    if (timer) {
        samd21_timer_set_ocmp(bsptimer, timer->expiry);
    } else {
        samd21_timer_disable_ocmp(bsptimer->tc_mod.hw);
    }

    cpu_irq_leave_critical();
}
#endif

/**
 * hal timer irq handler
 *
 * Generic HAL timer irq handler.
 *
 * @param tmr
 */
/**
 * hal timer irq handler
 *
 * This is the global timer interrupt routine.
 *
 */
#if (MYNEWT_VAL(TIMER_0) || MYNEWT_VAL(TIMER_1) || MYNEWT_VAL(TIMER_2))
static void
hal_timer_irq_handler(struct samd21_hal_timer *bsptimer)
{
    uint8_t compare;
    uint8_t ovf_int;
    Tc *hwtimer;

    /* Check interrupt source. If set, clear them */
    hwtimer = bsptimer->tc_mod.hw;
    compare = hwtimer->COUNT16.INTFLAG.reg & TC_INTFLAG_MC0;
    if (compare) {
        hwtimer->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;
    }

    ovf_int = hwtimer->COUNT16.INTFLAG.reg & TC_INTFLAG_OVF;
    if (ovf_int) {
        hwtimer->COUNT16.INTFLAG.reg = TC_INTFLAG_OVF;
        bsptimer->tmr_cntr += 65536;
    }


    /* XXX: make these stats? */
    /* Count # of timer isrs */
    ++bsptimer->timer_isrs;

    /*
     * NOTE: we dont check the 'compare' variable here due to how the timer
     * is implemented on this chip. There is no way to force an output
     * compare, so if we are late setting the output compare (i.e. the timer
     * counter is already passed the output compare value), we use the NVIC
     * to set a pending interrupt. This means that there will be no compare
     * flag set, so all we do is check to see if the compare interrupt is
     * enabled.
     */
    hal_timer_chk_queue(bsptimer);
    compare = hwtimer->COUNT16.INTFLAG.reg;
}
#endif

#if MYNEWT_VAL(TIMER_0)
void
samd21_timer0_irq_handler(void)
{
    hal_timer_irq_handler(&samd21_hal_timer0);
}
#endif

#if MYNEWT_VAL(TIMER_1)
void
samd21_timer1_irq_handler(void)
{
    hal_timer_irq_handler(&samd21_hal_timer1);
}
#endif

#if MYNEWT_VAL(TIMER_2)
void
samd21_timer2_irq_handler(void)
{
    hal_timer_irq_handler(&samd21_hal_timer2);
}
#endif

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
hal_timer_init(int timer_num, void *cfg)
{
    int rc;
    uint8_t irq_num;
    struct samd21_hal_timer *bsptimer;
    struct samd21_timer_cfg *tmr_cfg;
    hal_timer_irq_handler_t irq_isr;
    struct system_gclk_gen_config gcfg;

    /* Get timer. Make sure not enabled */
    SAMD21_HAL_TIMER_RESOLVE(timer_num, bsptimer);
    if (bsptimer->tmr_enabled) {
        rc = EINVAL;
        goto err;
    }
    tmr_cfg = (struct samd21_timer_cfg *)cfg;

    rc = 0;
    switch (timer_num) {
#if MYNEWT_VAL(TIMER_0)
    case 0:
        irq_isr = samd21_timer0_irq_handler;
        break;
#endif
#if MYNEWT_VAL(TIMER_1)
    case 1:
        irq_isr = samd21_timer1_irq_handler;
        break;
#endif
#if MYNEWT_VAL(TIMER_2)
    case 2:
        irq_isr = samd21_timer2_irq_handler;
        break;
#endif
    default:
        rc = -1;
        break;
    }

    if (rc) {
        goto err;
    }

    /* set up gclk generator to source this timer */
    gcfg.division_factor = 1;
    gcfg.high_when_disabled = false;
    gcfg.output_enable = false;
    gcfg.run_in_standby = true;
    gcfg.source_clock = tmr_cfg->src_clock;
    system_gclk_gen_set_config(tmr_cfg->clkgen, &gcfg);

    irq_num = tmr_cfg->irq_num;
    bsptimer->tmr_irq_num = irq_num;
    bsptimer->tmr_srcclk = tmr_cfg->src_clock;
    bsptimer->tmr_clkgen = tmr_cfg->clkgen;
    bsptimer->tc_mod.hw = tmr_cfg->hwtimer;
    bsptimer->tmr_initialized = 1;

    NVIC_DisableIRQ(irq_num);
    NVIC_SetPriority(irq_num, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_SetVector(irq_num, (uint32_t)irq_isr);

    tc_disable(&bsptimer->tc_mod);

    return 0;

err:
    return rc;
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
    int rc;
    uint8_t prescaler;
    uint16_t prescaler_reg;
    uint32_t div;
    uint32_t min_delta;
    uint32_t max_delta;
    uint32_t max_frequency;
    struct samd21_hal_timer *bsptimer;
    enum status_code tc_rc;
    struct tc_config cfg;

    /* Get timer. Make sure not enabled */
    SAMD21_HAL_TIMER_RESOLVE(timer_num, bsptimer);
    if (bsptimer->tmr_enabled || (bsptimer->tmr_initialized == 0) ||
        (freq_hz == 0)) {
        rc = EINVAL;
        goto err;
    }

    /* Set tc config to default values */
    tc_get_config_defaults(&cfg);

    /* Determine max frequency based on clock source */
    rc = 0;
    switch (bsptimer->tmr_srcclk) {
    case GCLK_SOURCE_DFLL48M:
        max_frequency = 48000000;
        break;
    case GCLK_SOURCE_FDPLL:
        max_frequency = 96000000;
        break;
    case GCLK_SOURCE_OSCULP32K:
    case GCLK_SOURCE_OSC32K:
    case GCLK_SOURCE_XOSC32K:
        max_frequency = 32768;
        break;
    case GCLK_SOURCE_OSC8M:
        max_frequency = 8000000;
        break;
    default:
        max_frequency = 0;
        rc = -1;
        break;
    }
    div = max_frequency / freq_hz;

    if (rc || (freq_hz > max_frequency) || (div == 0) || (div > 1024)) {
        goto err;
    }

    /* Set up timer counter. Need to determine prescaler */
    cfg.counter_size = TC_COUNTER_SIZE_16BIT;

    if (div == 1) {
        prescaler = 0;
        prescaler_reg = 0;
    } else {
        /* Find closest prescaler */
        prescaler = 1;
        prescaler_reg = 1;
        while (prescaler < 11) {
            if (div <= (1 << prescaler)) {
                min_delta = div - (1 << (prescaler - 1));
                max_delta = (1 << prescaler) - div;
                if (min_delta < max_delta) {
                    prescaler -= 1;
                }
                break;
            }
            if (prescaler < 4) {
                ++prescaler;
            } else {
                prescaler += 2;
            }
            ++prescaler_reg;
        }
    }
    cfg.clock_prescaler = prescaler_reg << TC_CTRLA_PRESCALER_Pos;
    cfg.clock_source = bsptimer->tmr_clkgen;

    /* set up gclk generator to source this timer */
    system_gclk_gen_enable(bsptimer->tmr_clkgen);

    tc_rc = tc_init(&bsptimer->tc_mod, bsptimer->tc_mod.hw, &cfg);
    if (tc_rc == STATUS_OK) {
        bsptimer->tc_mod.hw->COUNT16.INTENSET.reg = TC_INTFLAG_OVF;
        tc_enable(&bsptimer->tc_mod);
    } else {
        rc = EINVAL;
        goto err;
    }

    /* Now set the actual frequency */
    bsptimer->tmr_freq = max_frequency / (1 << prescaler);
    bsptimer->tmr_enabled = 1;

    /* Set isr in vector table and enable interrupt */
    NVIC_EnableIRQ(bsptimer->tmr_irq_num);

    return 0;

err:
    return rc;
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
    int rc;
    struct samd21_hal_timer *bsptimer;

    SAMD21_HAL_TIMER_RESOLVE(timer_num, bsptimer);

    tc_disable(&bsptimer->tc_mod);
    system_gclk_gen_disable(bsptimer->tmr_clkgen);
    bsptimer->tmr_enabled = 0;
    bsptimer->tmr_initialized = 0;

err:
    return rc;
}

/**
 * hal timer get resolution
 *
 * Get the resolution of the timer. This is the timer period, in nanoseconds
 *
 * @param timer_num
 *
 * @return uint32_t The resolution of the timer, in nanoseconds.
 */
uint32_t
hal_timer_get_resolution(int timer_num)
{
    int rc;
    uint32_t resolution;
    struct samd21_hal_timer *bsptimer;

    SAMD21_HAL_TIMER_RESOLVE(timer_num, bsptimer);

    resolution = 1000000000 / bsptimer->tmr_freq;
    return resolution;

err:
    rc = 0;
    return rc;
}

/**
 * hal timer read
 *
 * Returns the timer counter. NOTE: if the timer is a 16-bit timer, only
 * the lower 16 bits are valid. If the timer is a 64-bit timer, only the
 * low 32-bits are returned.
 *
 * @return uint32_t The timer counter register.
 */
uint32_t
hal_timer_read(int timer_num)
{
    int rc;
    uint32_t tcntr;
    struct samd21_hal_timer *bsptimer;

    SAMD21_HAL_TIMER_RESOLVE(timer_num, bsptimer);
    tcntr = hal_timer_read_bsptimer(bsptimer);
    return tcntr;

    /* Assert here since there is no invalid return code */
err:
    assert(0);
    rc = 0;
    return rc;
}

/**
 * hal timer delay
 *
 * Blocking delay for n ticks
 *
 * @param timer_num
 * @param ticks
 *
 * @return int 0 on success; error code otherwise.
 */
int
hal_timer_delay(int timer_num, uint32_t ticks)
{
    uint32_t until;

    until = hal_timer_read(timer_num) + ticks;
    while ((int32_t)(hal_timer_read(timer_num) - until) <= 0) {
        /* Loop here till finished */
    }

    return 0;
}

/**
 *
 * Initialize the HAL timer structure with the callback and the callback
 * argument. Also initializes the HW specific timer pointer.
 *
 * @param cb_func
 *
 * @return int
 */
int
hal_timer_set_cb(int timer_num, struct hal_timer *timer, hal_timer_cb cb_func,
                 void *arg)
{
    int rc;
    struct samd21_hal_timer *bsptimer;

    SAMD21_HAL_TIMER_RESOLVE(timer_num, bsptimer);

    timer->cb_func = cb_func;
    timer->cb_arg = arg;
    timer->link.tqe_prev = NULL;
    timer->bsp_timer = bsptimer;

    rc = 0;

err:
    return rc;
}

int
hal_timer_start(struct hal_timer *timer, uint32_t ticks)
{
    int rc;
    uint32_t tick;
    struct samd21_hal_timer *bsptimer;

    /* Set the tick value at which the timer should expire */
    bsptimer = (struct samd21_hal_timer *)timer->bsp_timer;
    tick = hal_timer_read_bsptimer(bsptimer) + ticks;
    rc = hal_timer_start_at(timer, tick);
    return rc;
}

int
hal_timer_start_at(struct hal_timer *timer, uint32_t tick)
{
    struct hal_timer *entry;
    struct samd21_hal_timer *bsptimer;

    if ((timer == NULL) || (timer->link.tqe_prev != NULL) ||
        (timer->cb_func == NULL)) {
        return EINVAL;
    }
    bsptimer = (struct samd21_hal_timer *)timer->bsp_timer;
    timer->expiry = tick;

    cpu_irq_enter_critical();

    if (TAILQ_EMPTY(&bsptimer->hal_timer_q)) {
        TAILQ_INSERT_HEAD(&bsptimer->hal_timer_q, timer, link);
    } else {
        TAILQ_FOREACH(entry, &bsptimer->hal_timer_q, link) {
            if ((int32_t)(timer->expiry - entry->expiry) < 0) {
                TAILQ_INSERT_BEFORE(entry, timer, link);
                break;
            }
        }
        if (!entry) {
            TAILQ_INSERT_TAIL(&bsptimer->hal_timer_q, timer, link);
        }
    }

    /* If this is the head, we need to set new OCMP */
    if (timer == TAILQ_FIRST(&bsptimer->hal_timer_q)) {
        samd21_timer_set_ocmp(bsptimer, timer->expiry);
    }

    cpu_irq_leave_critical();

    return 0;
}

/**
 * hal timer stop
 *
 * Stop a timer.
 *
 * @param timer
 *
 * @return int
 */
int
hal_timer_stop(struct hal_timer *timer)
{
    int reset_ocmp;
    struct hal_timer *entry;
    struct samd21_hal_timer *bsptimer;

    if (timer == NULL) {
        return EINVAL;
    }

    bsptimer = (struct samd21_hal_timer *)timer->bsp_timer;

    cpu_irq_enter_critical();

    if (timer->link.tqe_prev != NULL) {
        reset_ocmp = 0;
        if (timer == TAILQ_FIRST(&bsptimer->hal_timer_q)) {
            /* If first on queue, we will need to reset OCMP */
            entry = TAILQ_NEXT(timer, link);
            reset_ocmp = 1;
        }
        TAILQ_REMOVE(&bsptimer->hal_timer_q, timer, link);
        timer->link.tqe_prev = NULL;
        if (reset_ocmp) {
            if (entry) {
                samd21_timer_set_ocmp((struct samd21_hal_timer *)entry->bsp_timer,
                                      entry->expiry);
            } else {
                samd21_timer_disable_ocmp(bsptimer->tc_mod.hw);
            }
        }
    }

    cpu_irq_leave_critical();

    return 0;
}
