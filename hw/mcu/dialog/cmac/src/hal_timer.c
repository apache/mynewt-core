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
#include <stdint.h>
#include "syscfg/syscfg.h"
#include "mcu/mcu.h"
#include "mcu/cmac_hal.h"
#include "mcu/cmac_timer.h"
#include "hal/hal_timer.h"
#include "sys/queue.h"
#include "defs/error.h"
#include "os/os_arch.h"
#include "os/util.h"

#define TICKS_GT(_t1, _t2)          ((int64_t)((_t1) - (_t2)) > 0)
#define TICKS_GTE(_t1, _t2)         ((int64_t)((_t1) - (_t2)) >= 0)
#define TICKS_LT(_t1, _t2)          ((int64_t)((_t1) - (_t2)) < 0)
#define TICKS_LTE(_t1, _t2)         ((int64_t)((_t1) - (_t2)) <= 0)

static TAILQ_HEAD(hal_timer_qhead, hal_timer) g_hal_timer_queue;

/*
 * To avoid converting back and forth between uint64_t values returned by
 * cmac_timer and uint32_t representing hal_timer ticks, we compare cmac_timer
 * ticks everywhere. For this we need to store expiry value as uint64_t, so we
 * use bsp_timer (which is not used here since we only have one timer) pointer
 * to store high word of value.
 */

static inline uint64_t
hal_timer_expiry_get(struct hal_timer *timer)
{
    uint64_t ret;

    ret = (uint64_t)POINTER_TO_UINT(timer->bsp_timer) << 10;
    ret |= timer->expiry;

    return ret;
}

static inline void
hal_timer_expiry_set(struct hal_timer *timer, uint64_t val)
{
    timer->expiry = val & 0x3ff;
    timer->bsp_timer = UINT_TO_POINTER(val >> 10);
}

static void
hal_timer_check_queue(void)
{
    os_sr_t sr;
    struct hal_timer *e;
    uint64_t ll_timer_val;

    OS_ENTER_CRITICAL(sr);

    while ((e = TAILQ_FIRST(&g_hal_timer_queue)) != NULL) {
        ll_timer_val = cmac_timer_read64();
        if (TICKS_GT(hal_timer_expiry_get(e), ll_timer_val)) {
            break;
        }

        TAILQ_REMOVE(&g_hal_timer_queue, e, link);
        e->link.tqe_prev = NULL;
        e->cb_func(e->cb_arg);
    }

    if (e != NULL) {
        cmac_timer_write_eq_hal_timer(hal_timer_expiry_get(e));
    } else {
        cmac_timer_disable_eq_hal_timer();
    }

    OS_EXIT_CRITICAL(sr);
}

static void
hal_timer_cmac_timer_cb(void)
{
#if MYNEWT_VAL(TIMER_0)
    hal_timer_check_queue();
#endif
}

int
hal_timer_init(int timer_num, void *cfg)
{
    assert(timer_num == 0);

    cmac_timer_int_hal_timer_register(hal_timer_cmac_timer_cb);

    TAILQ_INIT(&g_hal_timer_queue);

    return 0;
}

int
hal_timer_config(int timer_num, uint32_t freq_hz)
{
    assert(timer_num == 0);
    assert(freq_hz == 32768);

    return 0;
}

int
hal_timer_set_cb(int timer_num, struct hal_timer *timer, hal_timer_cb func,
                 void *arg)
{
    assert(timer_num == 0);

    timer->cb_func = func;
    timer->cb_arg = arg;
    timer->link.tqe_prev = NULL;

    return 0;
}

int
hal_timer_start_at(struct hal_timer *timer, uint32_t tick)
{
    struct hal_timer *e;
    os_sr_t sr;
    uint64_t expiry;

    assert(timer);
    assert(timer->link.tqe_prev == NULL);
    assert(timer->cb_func);

    expiry = cmac_timer_convert_hal2llt(tick);
    hal_timer_expiry_set(timer, expiry);

    OS_ENTER_CRITICAL(sr);

    if (TAILQ_EMPTY(&g_hal_timer_queue)) {
        TAILQ_INSERT_HEAD(&g_hal_timer_queue, timer, link);
    } else {
        TAILQ_FOREACH(e, &g_hal_timer_queue, link) {
            if (TICKS_LT(expiry, hal_timer_expiry_get(e))) {
                TAILQ_INSERT_BEFORE(e, timer, link);
                break;
            }
        }
        if (!e) {
            TAILQ_INSERT_TAIL(&g_hal_timer_queue, timer, link);
        }
    }

    if (timer == TAILQ_FIRST(&g_hal_timer_queue)) {
        cmac_timer_write_eq_hal_timer(expiry);
    }

    OS_EXIT_CRITICAL(sr);

    return 0;
}

int
hal_timer_stop(struct hal_timer *timer)
{
    os_sr_t sr;
    bool reset_timer;

    assert(timer);

    if (timer->link.tqe_prev == NULL) {
        return 0;
    }

    OS_ENTER_CRITICAL(sr);

    reset_timer = (timer == TAILQ_FIRST(&g_hal_timer_queue));

    TAILQ_REMOVE(&g_hal_timer_queue, timer, link);
    timer->link.tqe_prev = NULL;

    if (reset_timer) {
        timer = TAILQ_FIRST(&g_hal_timer_queue);
        if (timer != NULL) {
            cmac_timer_write_eq_hal_timer(hal_timer_expiry_get(timer));
        } else {
            cmac_timer_disable_eq_hal_timer();
        }
    }

    OS_EXIT_CRITICAL(sr);

    return 0;
}

uint32_t
hal_timer_read(int timer_num)
{
    assert(timer_num == 0);

    return cmac_timer_convert_llt2hal(cmac_timer_read64());
}
