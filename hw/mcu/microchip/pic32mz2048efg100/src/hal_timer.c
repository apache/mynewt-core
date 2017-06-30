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

#include <os/os.h>
#include <stdint.h>
#include <xc.h>
#include "hal/hal_timer.h"
#include "mcu/mips_hal.h"

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
    TAILQ_HEAD(hal_timer_qhead, hal_timer) hal_timer_queue;
};
static struct pic32_timer timers[PIC32MZ_TIMER_COUNT];


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
    return MYNEWT_VAL(CLOCK_FREQ) / ((PB3DIV & _PB3DIV_PBDIV_MASK) + 1);
}

static void
hal_timer_enable_int(int timer_num)
{
    switch (timer_num) {
        case 0:
            IPC2CLR = _IPC2_T2IP_MASK | _IPC2_T2IS_MASK;
            IPC2SET = 3 << _IPC2_T2IP_POSITION;
            IFS0CLR = _IFS0_T2IF_MASK;
            IEC0SET = _IEC0_T2IE_MASK;
            break;
        case 1:
            IPC3CLR = _IPC3_T3IP_MASK | _IPC3_T3IS_MASK;
            IPC3SET = 3 << _IPC3_T3IP_POSITION;
            IFS0CLR = _IFS0_T3IF_MASK;
            IEC0SET = _IEC0_T3IE_MASK;
            break;
        case 2:
            IPC4CLR = _IPC4_T4IP_MASK | _IPC4_T4IS_MASK;
            IPC4SET = 3 << _IPC4_T4IP_POSITION;
            IFS0CLR = _IFS0_T4IF_MASK;
            IEC0SET = _IEC0_T4IE_MASK;
            break;
        case 3:
            IPC6CLR = _IPC6_T5IP_MASK | _IPC6_T5IS_MASK;
            IPC6SET = 3 << _IPC6_T5IP_POSITION;
            IFS0CLR = _IFS0_T5IF_MASK;
            IEC0SET = _IEC0_T5IE_MASK;
            break;
        case 4:
            IPC7CLR = _IPC7_T6IP_MASK | _IPC7_T6IS_MASK;
            IPC7SET = 3 << _IPC7_T6IP_POSITION;
            IFS0CLR = _IFS0_T6IF_MASK;
            IEC0SET = _IEC0_T6IE_MASK;
            break;
        case 5:
            IPC8CLR = _IPC8_T7IP_MASK | _IPC8_T7IS_MASK;
            IPC8SET = 3 << _IPC8_T7IP_POSITION;
            IFS1CLR = _IFS1_T7IF_MASK;
            IEC1SET = _IEC1_T7IE_MASK;
            break;
        case 6:
            IPC9CLR = _IPC9_T8IP_MASK | _IPC9_T8IS_MASK;
            IPC9SET = 3 << _IPC9_T8IP_POSITION;
            IFS1CLR = _IFS1_T8IF_MASK;
            IEC1SET = _IEC1_T8IE_MASK;
            break;
        case 7:
            IPC10CLR = _IPC10_T9IP_MASK | _IPC10_T9IS_MASK;
            IPC10SET = 3 << _IPC10_T9IP_POSITION;
            IFS1CLR = _IFS1_T9IF_MASK;
            IEC1SET = _IEC1_T9IE_MASK;
            break;
    }
}

static void
hal_timer_disable_int(int timer_num)
{
    switch (timer_num) {
        case 0:
            IEC0CLR = _IEC0_T2IE_MASK;
            IFS0CLR = _IFS0_T2IF_MASK;
            break;
        case 1:
            IEC0CLR = _IEC0_T3IE_MASK;
            IFS0CLR = _IFS0_T3IF_MASK;
            break;
        case 2:
            IEC0CLR = _IEC0_T4IE_MASK;
            IFS0CLR = _IFS0_T4IF_MASK;
            break;
        case 3:
            IEC0CLR = _IEC0_T5IE_MASK;
            IFS0CLR = _IFS0_T5IF_MASK;
            break;
        case 4:
            IEC0CLR = _IEC0_T6IE_MASK;
            IFS0CLR = _IFS0_T6IF_MASK;
            break;
        case 5:
            IEC1CLR = _IEC1_T7IE_MASK;
            IFS1CLR = _IFS1_T7IF_MASK;
            break;
        case 6:
            IEC1CLR = _IEC1_T8IE_MASK;
            IFS1CLR = _IFS1_T8IF_MASK;
            break;
        case 7:
            IEC1CLR = _IEC1_T9IE_MASK;
            IFS1CLR = _IFS1_T9IF_MASK;
            break;
    }

}

static void
update_period_register(int timer_num)
{
    if (TAILQ_EMPTY(&timers[timer_num].hal_timer_queue)) {
        PRx(timer_num) = UINT16_MAX;
    } else {
        struct hal_timer *first = TAILQ_FIRST(
            &timers[timer_num].hal_timer_queue);
        uint32_t ticks = hal_timer_read(timer_num);

        if (ticks >= first->expiry) {
            /*
             * Create a timer interrupt immediately. This case must never
             * execute inside the interrupt handler (otherwise we would skip
             * the interrupt).
             */
            PRx(timer_num) = TMRx(timer_num) + 1;
        } else {
            uint32_t delta = ticks - first->expiry;
            if (delta > UINT16_MAX)
                delta = UINT16_MAX;

            PRx(timer_num) = delta;
        }
    }
}

static inline void
update_counter(int timer_num)
{
    timers[timer_num].counter += PRx(timer_num);
}

static void
handle_timer_list(int timer_num)
{
    uint32_t current_tick = hal_timer_read(timer_num);
    struct hal_timer *entry;

    while ((entry = TAILQ_FIRST(&timers[timer_num].hal_timer_queue)) !=
           NULL) {
        if (entry->expiry <= current_tick) {
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

void
__attribute__((interrupt(IPL3AUTO), vector(_TIMER_2_VECTOR)))
timer2_isr(void)
{
    update_counter(0);
    handle_timer_list(0);

    IFS0CLR = _IFS0_T2IF_MASK;
}

void
__attribute__((interrupt(IPL3AUTO), vector(_TIMER_3_VECTOR)))
timer3_isr(void)
{
    update_counter(1);
    handle_timer_list(1);

    IFS0CLR = _IFS0_T3IF_MASK;
}

void
__attribute__((interrupt(IPL3AUTO), vector(_TIMER_4_VECTOR)))
timer4_isr(void)
{
    update_counter(2);
    handle_timer_list(2);

    IFS0CLR = _IFS0_T4IF_MASK;
}

void
__attribute__((interrupt(IPL3AUTO), vector(_TIMER_5_VECTOR)))
timer5_isr(void)
{
    update_counter(3);
    handle_timer_list(3);

    IFS0CLR = _IFS0_T5IF_MASK;
}

void
__attribute__((interrupt(IPL3AUTO), vector(_TIMER_6_VECTOR)))
timer6_isr(void)
{
    update_counter(4);
    handle_timer_list(4);

    IFS0CLR = _IFS0_T6IF_MASK;
}

void
__attribute__((interrupt(IPL3AUTO), vector(_TIMER_7_VECTOR)))
timer7_isr(void)
{
    update_counter(5);
    handle_timer_list(5);

    IFS1CLR = _IFS1_T7IF_MASK;
}

void
__attribute__((interrupt(IPL3AUTO), vector(_TIMER_8_VECTOR)))
timer8_isr(void)
{
    update_counter(6);
    handle_timer_list(6);

    IFS1CLR = _IFS1_T8IF_MASK;
}

void
__attribute__((interrupt(IPL3AUTO), vector(_TIMER_9_VECTOR)))
timer9_isr(void)
{
    update_counter(7);
    handle_timer_list(7);

    IFS1CLR = _IFS1_T9IF_MASK;
}

int
hal_timer_init(int timer_num, void *cfg)
{
    if (timer_num >= PIC32MZ_TIMER_COUNT)
        return -1;

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
    uint32_t pr;
    uint32_t ideal_prescaler;

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
                uint32_t min_delta = ideal_prescaler - timer_prescalers[i - 1];
                uint32_t max_delta = timer_prescalers[i] - ideal_prescaler;
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

    tmr = TMRx(timer_num);
    counter = timers[timer_num].counter;

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
        struct hal_timer *entry;
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