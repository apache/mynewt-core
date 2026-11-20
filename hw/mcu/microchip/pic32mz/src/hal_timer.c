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

#include <stdint.h>
#include <string.h>
#include <os/mynewt.h>
#include <xc.h>
#include <mcu/pic32.h>
#include <hal/hal_timer.h>
#include <mcu/mips_hal.h>
#include <mcu/pic32_isr.h>

#define PIC32MZ_TIMER_COUNT         (8)
#define PIC32MZ_PRESCALER_COUNT     (8)

#define TxCON(T)        (base_address[T][0x0 / 0x4])
#define TxCONCLR(T)     (base_address[T][0x4 / 0x4])
#define TxCONSET(T)     (base_address[T][0x8 / 0x4])
#define TMRx(T)         (base_address[T][0x10 / 0x4])
#define PRx(T)          (base_address[T][0x20 / 0x4])

static volatile uint32_t * base_address[PIC32MZ_TIMER_COUNT] = {
    (volatile uint32_t *)_TMR2_BASE_ADDRESS,
    (volatile uint32_t *)_TMR3_BASE_ADDRESS,
    (volatile uint32_t *)_TMR4_BASE_ADDRESS,
    (volatile uint32_t *)_TMR5_BASE_ADDRESS,
    (volatile uint32_t *)_TMR6_BASE_ADDRESS,
    (volatile uint32_t *)_TMR7_BASE_ADDRESS,
    (volatile uint32_t *)_TMR8_BASE_ADDRESS,
    (volatile uint32_t *)_TMR9_BASE_ADDRESS
};

static uint32_t timer_prescalers[PIC32MZ_PRESCALER_COUNT] =
    {1, 2, 4, 8, 16, 32, 64, 256};

struct pic32_timer {
    uint32_t index;
    uint32_t counter;
    uint32_t frequency;     /* Holds the true frequency of the timer */
    uint32_t min_ticks;     /* Set by hal_timer_config() */
    TAILQ_HEAD(hal_timer_qhead, hal_timer) hal_timer_queue;
};
static struct pic32_timer timers[PIC32MZ_TIMER_COUNT];

/* Word index of register and its CLR, SET, INV variants */
#define REG             0
#define REG_CLR         1
#define REG_SET         2
#define REG_INV         3

/* Interrupt flag register (IFSx) and flag mask of each timer */
struct timer_ifs {
    volatile uint32_t *reg;
    uint32_t mask;
};

static const struct timer_ifs timer_ifs[PIC32MZ_TIMER_COUNT] = {
    { &IFS0, _IFS0_T2IF_MASK },
    { &IFS0, _IFS0_T3IF_MASK },
    { &IFS0, _IFS0_T4IF_MASK },
    { &IFS0, _IFS0_T5IF_MASK },
    { &IFS0, _IFS0_T6IF_MASK },
    { &IFS1, _IFS1_T7IF_MASK },
    { &IFS1, _IFS1_T8IF_MASK },
    { &IFS1, _IFS1_T9IF_MASK },
};

/* Period match happened, interrupt not handled yet */
static inline bool
timer_int_pending(int timer_num)
{
    return (timer_ifs[timer_num].reg[REG] & timer_ifs[timer_num].mask) != 0;
}

static inline void
timer_int_clear(int timer_num)
{
    timer_ifs[timer_num].reg[REG_CLR] = timer_ifs[timer_num].mask;
}

static inline uint32_t
hal_timer_get_prescaler(int timer_num)
{
    uint32_t index =
        (TxCON(timer_num) & _T2CON_TCKPS_MASK) >> _T2CON_TCKPS_POSITION;
    return timer_prescalers[index];
}

static inline uint32_t
hal_timer_get_peripheral_base_clock(void)
{
    return SystemCoreClock / ((PB3DIV & _PB3DIV_PBDIV_MASK) + 1);
}

static void
hal_timer_enable_int(int timer_num)
{
    switch (timer_num) {
#if MYNEWT_VAL(TIMER_0)
    case 0:
        IFS0CLR = _IFS0_T2IF_MASK;
        IEC0SET = _IEC0_T2IE_MASK;
        break;
#endif
#if MYNEWT_VAL(TIMER_1)
    case 1:
        IFS0CLR = _IFS0_T3IF_MASK;
        IEC0SET = _IEC0_T3IE_MASK;
        break;
#endif
#if MYNEWT_VAL(TIMER_2)
    case 2:
        IFS0CLR = _IFS0_T4IF_MASK;
        IEC0SET = _IEC0_T4IE_MASK;
        break;
#endif
#if MYNEWT_VAL(TIMER_3)
    case 3:
        IFS0CLR = _IFS0_T5IF_MASK;
        IEC0SET = _IEC0_T5IE_MASK;
        break;
#endif
#if MYNEWT_VAL(TIMER_4)
    case 4:
        IFS0CLR = _IFS0_T6IF_MASK;
        IEC0SET = _IEC0_T6IE_MASK;
        break;
#endif
#if MYNEWT_VAL(TIMER_5)
    case 5:
        IFS1CLR = _IFS1_T7IF_MASK;
        IEC1SET = _IEC1_T7IE_MASK;
        break;
#endif
#if MYNEWT_VAL(TIMER_6)
    case 6:
        IFS1CLR = _IFS1_T8IF_MASK;
        IEC1SET = _IEC1_T8IE_MASK;
        break;
#endif
#if MYNEWT_VAL(TIMER_7)
    case 7:
        IFS1CLR = _IFS1_T9IF_MASK;
        IEC1SET = _IEC1_T9IE_MASK;
        break;
#endif
    }
}

static void
hal_timer_disable_int(int timer_num)
{
    switch (timer_num) {
#if MYNEWT_VAL(TIMER_0)
    case 0:
        IEC0CLR = _IEC0_T2IE_MASK;
        IFS0CLR = _IFS0_T2IF_MASK;
        break;
#endif
#if MYNEWT_VAL(TIMER_1)
    case 1:
        IEC0CLR = _IEC0_T3IE_MASK;
        IFS0CLR = _IFS0_T3IF_MASK;
        break;
#endif
#if MYNEWT_VAL(TIMER_2)
    case 2:
        IEC0CLR = _IEC0_T4IE_MASK;
        IFS0CLR = _IFS0_T4IF_MASK;
        break;
#endif
#if MYNEWT_VAL(TIMER_3)
    case 3:
        IEC0CLR = _IEC0_T5IE_MASK;
        IFS0CLR = _IFS0_T5IF_MASK;
        break;
#endif
#if MYNEWT_VAL(TIMER_4)
    case 4:
        IEC0CLR = _IEC0_T6IE_MASK;
        IFS0CLR = _IFS0_T6IF_MASK;
        break;
#endif
#if MYNEWT_VAL(TIMER_5)
    case 5:
        IEC1CLR = _IEC1_T7IE_MASK;
        IFS1CLR = _IFS1_T7IF_MASK;
        break;
#endif
#if MYNEWT_VAL(TIMER_6)
    case 6:
        IEC1CLR = _IEC1_T8IE_MASK;
        IFS1CLR = _IFS1_T8IF_MASK;
        break;
#endif
#if MYNEWT_VAL(TIMER_7)
    case 7:
        IEC1CLR = _IEC1_T9IE_MASK;
        IFS1CLR = _IFS1_T9IF_MASK;
        break;
#endif
    }
}

/*
 * TMRx counts from 0 to PRx, then resets to 0 and sets interrupt flag, one
 * period is PRx + 1 ticks.  PRx is set so that period ends at expiry of
 * first timer in the queue (or at most 0x10000 ticks).  Must be called with
 * interrupts disabled.
 */
static void
update_period_register(int timer_num)
{
    struct hal_timer *first;
    uint32_t tmr;
    uint32_t pr;
    int32_t delta;

    /*
     * Period already ended, interrupt handler will add current PRx to
     * counter and then set new period.
     */
    if (timer_int_pending(timer_num)) {
        return;
    }

    if (TAILQ_EMPTY(&timers[timer_num].hal_timer_queue)) {
        PRx(timer_num) = UINT16_MAX;
        return;
    }

    first = TAILQ_FIRST(&timers[timer_num].hal_timer_queue);
    tmr = TMRx(timer_num);
    delta = (int32_t)(first->expiry - (timers[timer_num].counter + tmr));

    /* New period end must still be ahead of TMRx when PRx is written */
    if (delta < (int32_t)timers[timer_num].min_ticks) {
        delta = (int32_t)timers[timer_num].min_ticks;
    }
    pr = tmr + (uint32_t)delta;
    if (pr > UINT16_MAX) {
        pr = UINT16_MAX;
    }
    PRx(timer_num) = pr;

    /* Missed it anyway, end period at 0xFFFF so counter stays correct */
    if (TMRx(timer_num) >= pr && !timer_int_pending(timer_num)) {
        PRx(timer_num) = UINT16_MAX;
    }
}

static inline void
update_counter(int timer_num)
{
    timers[timer_num].counter += PRx(timer_num) + 1;
}

static void
handle_timer_list(int timer_num)
{
    uint32_t current_tick = hal_timer_read(timer_num);
    struct hal_timer *entry;

    while ((entry = TAILQ_FIRST(&timers[timer_num].hal_timer_queue)) !=
           NULL) {
        if ((int32_t)(entry->expiry - current_tick) <= 0) {
            TAILQ_REMOVE(&timers[timer_num].hal_timer_queue, entry, link);
            entry->link.tqe_prev = NULL;
            entry->link.tqe_next = NULL;
            entry->cb_func(entry->cb_arg);
        } else {
            break;
        }
    }

    /*
     * Even if the list is left unchanged, the period register still needs to
     * be computed again to ensure that the first callback in the list will
     * be called on time.
     */
    update_period_register(timer_num);
}

/*
 * Timer interrupts, priority (TIMER_n_IPL) is set by pic32_isr_init().
 * Timer callbacks are called with FPU disabled, they must not use floating
 * point.
 */
static void
timer_isr(int timer_num)
{
    timer_int_clear(timer_num);
    update_counter(timer_num);
    handle_timer_list(timer_num);
}

#if MYNEWT_VAL(TIMER_0)
static void
timer2_isr(void)
{
    timer_isr(0);
}
PIC32_ISR_NO_FPU(_TIMER_2_VECTOR, MYNEWT_VAL(TIMER_0_IPL), timer2_isr);
#endif

#if MYNEWT_VAL(TIMER_1)
static void
timer3_isr(void)
{
    timer_isr(1);
}
PIC32_ISR_NO_FPU(_TIMER_3_VECTOR, MYNEWT_VAL(TIMER_1_IPL), timer3_isr);
#endif

#if MYNEWT_VAL(TIMER_2)
static void
timer4_isr(void)
{
    timer_isr(2);
}
PIC32_ISR_NO_FPU(_TIMER_4_VECTOR, MYNEWT_VAL(TIMER_2_IPL), timer4_isr);
#endif

#if MYNEWT_VAL(TIMER_3)
static void
timer5_isr(void)
{
    timer_isr(3);
}
PIC32_ISR_NO_FPU(_TIMER_5_VECTOR, MYNEWT_VAL(TIMER_3_IPL), timer5_isr);
#endif

#if MYNEWT_VAL(TIMER_4)
static void
timer6_isr(void)
{
    timer_isr(4);
}
PIC32_ISR_NO_FPU(_TIMER_6_VECTOR, MYNEWT_VAL(TIMER_4_IPL), timer6_isr);
#endif

#if MYNEWT_VAL(TIMER_5)
static void
timer7_isr(void)
{
    timer_isr(5);
}
PIC32_ISR_NO_FPU(_TIMER_7_VECTOR, MYNEWT_VAL(TIMER_5_IPL), timer7_isr);
#endif

#if MYNEWT_VAL(TIMER_6)
static void
timer8_isr(void)
{
    timer_isr(6);
}
PIC32_ISR_NO_FPU(_TIMER_8_VECTOR, MYNEWT_VAL(TIMER_6_IPL), timer8_isr);
#endif

#if MYNEWT_VAL(TIMER_7)
static void
timer9_isr(void)
{
    timer_isr(7);
}
PIC32_ISR_NO_FPU(_TIMER_9_VECTOR, MYNEWT_VAL(TIMER_7_IPL), timer9_isr);
#endif

int
hal_timer_init(int timer_num, void *cfg)
{
    if (timer_num >= PIC32MZ_TIMER_COUNT) {
        return -1;
    }

    TxCON(timer_num) = 0;
    timers[timer_num].index = timer_num;
    timers[timer_num].counter = 0;

    hal_timer_enable_int(timer_num);

    return 0;
}

int
hal_timer_deinit(int timer_num)
{
    struct hal_timer *timer;

    if (timer_num >= PIC32MZ_TIMER_COUNT) {
        return -1;
    }

    TxCON(timer_num) = 0;
    hal_timer_disable_int(timer_num);
    while ((timer = TAILQ_FIRST(&timers[timer_num].hal_timer_queue)) !=
           NULL) {
        TAILQ_REMOVE(&timers[timer_num].hal_timer_queue, timer, link);
    }

    return 0;
}

int
hal_timer_config(int timer_num, uint32_t freq_hz)
{
    int i;
    uint32_t ideal_prescaler;
    uint32_t min_delta;
    uint32_t max_delta;

    if (timer_num >= PIC32MZ_TIMER_COUNT) {
        return -1;
    }

    if (freq_hz == 0) {
        return -1;
    }

    ideal_prescaler = hal_timer_get_peripheral_base_clock() / freq_hz;
    if (ideal_prescaler > 256) {
        return -1;
    }

    if (ideal_prescaler == 1) {
        i = 0;
    } else {
        /* Find closest prescaler */
        for (i = 1; i < PIC32MZ_PRESCALER_COUNT; ++i) {
            if (ideal_prescaler <= timer_prescalers[i]) {
                min_delta = ideal_prescaler - timer_prescalers[i - 1];
                max_delta = timer_prescalers[i] - ideal_prescaler;
                if (min_delta < max_delta) {
                    i -= 1;
                }
                break;
            }
        }
    }

    TxCON(timer_num) = 0;

    TxCONCLR(timer_num) = _T2CON_TCKPS_MASK;
    TxCONSET(timer_num) = (i << _T2CON_TCKPS_POSITION) & _T2CON_TCKPS_MASK;

    /* Set PR to its maximum value to minimize timer interrupts */
    PRx(timer_num) = UINT16_MAX;
    TMRx(timer_num) = 0;

    timers[timer_num].frequency = hal_timer_get_peripheral_base_clock() /
                                  timer_prescalers[i];
    /*
     * TMRx runs while new PRx is computed and written, if it passes PRx
     * before the write there is no match until TMRx wraps at 0xFFFF.
     * Allow about 1 us for that plus 2 ticks.
     */
    timers[timer_num].min_ticks = timers[timer_num].frequency / 1000000 + 2;

    /* Start timer */
    TxCONSET(timer_num) = _T2CON_TON_MASK;

    return 0;
}

uint32_t
hal_timer_get_resolution(int timer_num)
{
    if (timer_num >= PIC32MZ_TIMER_COUNT) {
        return 0;
    }

    return 1000000000 / timers[timer_num].frequency;
}

uint32_t
hal_timer_read(int timer_num)
{
    uint32_t tmr, counter, ctx;

    if (timer_num >= PIC32MZ_TIMER_COUNT) {
        return 0;
    }

    __HAL_DISABLE_INTERRUPTS(ctx);

    counter = timers[timer_num].counter;
    tmr = TMRx(timer_num);
    if (timer_int_pending(timer_num)) {
        /*
         * Period ended but interrupt was not handled yet, TMRx is read
         * again since it could be read just before it was reset.
         */
        tmr = TMRx(timer_num);
        counter += PRx(timer_num) + 1;
    }

    __HAL_ENABLE_INTERRUPTS(ctx);

    return tmr + counter;
}

int
hal_timer_delay(int timer_num, uint32_t ticks)
{
    uint32_t until;

    if (timer_num >= PIC32MZ_TIMER_COUNT) {
        return -1;
    }

    until = hal_timer_read(timer_num) + ticks;
    while ((int32_t)(hal_timer_read(timer_num) - until) <= 0) {
    }

    return 0;
}

int
hal_timer_set_cb(int timer_num, struct hal_timer *timer, hal_timer_cb cb_func,
                 void *arg)
{
    if (timer_num >= PIC32MZ_TIMER_COUNT) {
        return -1;
    }

    memset(timer, 0, sizeof(struct hal_timer));
    timer->bsp_timer = &timers[timer_num];
    timer->cb_func = cb_func;
    timer->cb_arg = arg;

    return 0;
}

int
hal_timer_start(struct hal_timer *timer, uint32_t ticks)
{
    uint32_t tick;
    struct pic32_timer *bsp_timer;

    if (timer == NULL || ticks == 0) {
        return -1;
    }

    bsp_timer = (struct pic32_timer *)(timer->bsp_timer);
    if (bsp_timer == NULL) {
        return -1;
    }

    tick = hal_timer_read(bsp_timer->index) + ticks;
    return hal_timer_start_at(timer, tick);
}

int
hal_timer_start_at(struct hal_timer *timer, uint32_t tick)
{
    os_sr_t ctx;
    struct pic32_timer *bsp_timer;
    struct hal_timer *entry;

    if ((timer == NULL) || (timer->link.tqe_prev != NULL) ||
        (timer->cb_func == NULL)) {
        return -1;
    }

    bsp_timer = (struct pic32_timer *)timer->bsp_timer;
    if (bsp_timer == NULL) {
        return -1;
    }

    timer->expiry = tick;

    __HAL_DISABLE_INTERRUPTS(ctx);

    /* Add to callback queue, order using expiry tick in increasing order */
    if (TAILQ_EMPTY(&bsp_timer->hal_timer_queue)) {
        TAILQ_INSERT_HEAD(&bsp_timer->hal_timer_queue, timer, link);
    } else {
        TAILQ_FOREACH(entry, &bsp_timer->hal_timer_queue, link) {
            if ((int32_t)(timer->expiry - entry->expiry) < 0) {
                TAILQ_INSERT_BEFORE(entry, timer, link);
                break;
            }
        }
        if (!entry) {
            TAILQ_INSERT_TAIL(&bsp_timer->hal_timer_queue, timer, link);
        }
    }

    update_period_register(bsp_timer->index);

    __HAL_ENABLE_INTERRUPTS(ctx);

    return 0;
}

int
hal_timer_stop(struct hal_timer *timer)
{
    os_sr_t ctx;
    struct pic32_timer *bsp_timer;

    if (timer == NULL) {
        return -1;
    }

    bsp_timer = (struct pic32_timer *)timer->bsp_timer;
    if (bsp_timer == NULL) {
        return -1;
    }

    __HAL_DISABLE_INTERRUPTS(ctx);

    if (timer->link.tqe_prev != NULL) {
        TAILQ_REMOVE(&bsp_timer->hal_timer_queue, timer, link);
        timer->link.tqe_prev = NULL;
        timer->link.tqe_next = NULL;
    }

    update_period_register(bsp_timer->index);

    __HAL_ENABLE_INTERRUPTS(ctx);

    return 0;
}
