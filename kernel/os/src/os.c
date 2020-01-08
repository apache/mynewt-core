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

#include "os/mynewt.h"
#include "os_priv.h"

#include "hal/hal_os_tick.h"
#include "hal/hal_bsp.h"
#include "hal/hal_system.h"
#include "hal/hal_watchdog.h"

#if MYNEWT_VAL(RTT)
#include "rtt/SEGGER_RTT.h"
#endif

/**
 * @defgroup OSKernel Operating System Kernel
 * @brief This section contains documentation for the core operating system kernel
 * of Apache Mynewt.
 * @{
 *   @addtogroup OSGeneral General Functions
 *   @{
 */

struct os_task g_idle_task;
OS_TASK_STACK_DEFINE(g_idle_task_stack, OS_IDLE_STACK_SIZE);

uint32_t g_os_idle_ctr;

struct os_task g_os_main_task;
OS_TASK_STACK_DEFINE(g_os_main_stack, OS_MAIN_STACK_SIZE);

/*
 * Double the interval timer to allow proper timer check-in.
 */
#define OS_MAIN_TASK_TIMER_TICKS \
    os_time_ms_to_ticks32(MYNEWT_VAL(OS_MAIN_TASK_SANITY_ITVL_MS)) * 2

#if MYNEWT_VAL(OS_WATCHDOG_MONITOR)

/*
 * This should fire just before hal watchdog would.
 * The timer fires 2 seconds before hardware watchdog; adjust this if
 * more time is needed to write the corefile.
 */
#define OS_WDOG_MONITOR_TMO     ((MYNEWT_VAL(WATCHDOG_INTERVAL) - 2000) * 1000)
#if MYNEWT_VAL(WATCHDOG_INTERVAL) < 4000
#error "Watchdog interval too small, must be at least 4000m"
#endif

static struct hal_timer os_wdog_monitor;
#endif

#if MYNEWT_VAL(WATCHDOG_INTERVAL) - 200 < MYNEWT_VAL(SANITY_INTERVAL)
#error "Watchdog interval - 200 < sanity interval"
#endif


/* Default zero.  Set by the architecture specific code when os is started.
 */
int g_os_started;

#define MIN_IDLE_TICKS  (MYNEWT_VAL(OS_IDLE_TICKLESS_MS_MIN) * OS_TICKS_PER_SEC / 1000)
#define MAX_IDLE_TICKS  (MYNEWT_VAL(OS_IDLE_TICKLESS_MS_MAX) * OS_TICKS_PER_SEC / 1000)

/**
 * Idle operating system task, runs when no other tasks are running.
 * The idle task operates in tickless mode, which means it looks for
 * the next time an event in the system needs to run, and then tells
 * the architecture specific functions to sleep until that time.
 *
 * @param arg unused
 */
void
os_idle_task(void *arg)
{
    os_sr_t sr;
    os_time_t now;
    os_time_t iticks, sticks, cticks;
    os_time_t sanity_last;
    os_time_t sanity_itvl_ticks;

    sanity_itvl_ticks = (MYNEWT_VAL(SANITY_INTERVAL) * OS_TICKS_PER_SEC) / 1000;
    sanity_last = 0;

    hal_watchdog_tickle();
#if MYNEWT_VAL(OS_WATCHDOG_MONITOR)
    os_cputime_timer_stop(&os_wdog_monitor);
    os_cputime_timer_relative(&os_wdog_monitor, OS_WDOG_MONITOR_TMO);
#endif

    while (1) {
        ++g_os_idle_ctr;

        now = os_time_get();
        if (OS_TIME_TICK_GT(now, sanity_last + sanity_itvl_ticks)) {
            os_sanity_run();
            /* Tickle the watchdog after successfully running sanity */
            hal_watchdog_tickle();
#if MYNEWT_VAL(OS_WATCHDOG_MONITOR)
            os_cputime_timer_stop(&os_wdog_monitor);
            os_cputime_timer_relative(&os_wdog_monitor, OS_WDOG_MONITOR_TMO);
#endif
            sanity_last = now;
        }

        OS_ENTER_CRITICAL(sr);
        now = os_time_get();
        sticks = os_sched_wakeup_ticks(now);
        cticks = os_callout_wakeup_ticks(now);
        iticks = min(sticks, cticks);
        /* Wakeup in time to run sanity as well from the idle context,
         * as the idle task does not schedule itself.
         */
        iticks = min(iticks, ((sanity_last + sanity_itvl_ticks) - now));

        if (iticks < MIN_IDLE_TICKS) {
            iticks = 0;
        } else if (iticks > MAX_IDLE_TICKS) {
            iticks = MAX_IDLE_TICKS;
        } else {
            /* NOTHING */
        }
        /* Tell the architecture specific support to put the processor to sleep
         * for 'n' ticks.
         */

        os_trace_idle();
        os_tick_idle(iticks);
        OS_EXIT_CRITICAL(sr);
    }
}

/**
 * Has the operating system started.
 *
 * @return 1 if the operating system has started, 0 if it hasn't
 */
int
os_started(void)
{
    return (g_os_started);
}

static void
os_main(void *arg)
{
    int (*fn)(int argc, char **argv) = arg;

#if !MYNEWT_VAL(SELFTEST)
    fn(0, NULL);
#else
    (void)fn;
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
#endif
    assert(0);
}

#if MYNEWT_VAL(OS_WATCHDOG_MONITOR)
static void
os_wdog_monitor_tmo(void *arg)
{
    /*
     * Hardware watchdog about to fire.
     * Stop in debugger/coredump/fault printout.
     */
    assert(0);
    while(1);
}
#endif

void
os_init_idle_task(void)
{
    int rc;

    rc = os_task_init(&g_idle_task, "idle", os_idle_task, NULL,
            OS_IDLE_PRIO, OS_WAIT_FOREVER, g_idle_task_stack,
            OS_STACK_ALIGN(OS_IDLE_STACK_SIZE));
    assert(rc == 0);

    /* Initialize sanity */
    rc = os_sanity_init();
    assert(rc == 0);

    rc = hal_watchdog_init(MYNEWT_VAL(WATCHDOG_INTERVAL));
    assert(rc == 0);

#if MYNEWT_VAL(OS_WATCHDOG_MONITOR)
    os_cputime_timer_init(&os_wdog_monitor, os_wdog_monitor_tmo, NULL);
    os_cputime_timer_relative(&os_wdog_monitor, OS_WDOG_MONITOR_TMO);
#endif
}

void
os_init(int (*main_fn)(int argc, char **arg))
{
    os_error_t err;

#if MYNEWT_VAL(RTT)
    memset(&_SEGGER_RTT, 0, sizeof(_SEGGER_RTT));
    SEGGER_RTT_Init();
#endif

    TAILQ_INIT(&g_callout_list);
    STAILQ_INIT(&g_os_task_list);
    os_eventq_init(os_eventq_dflt_get());

    /* Initialize device list. */
    os_dev_reset();

    err = os_arch_os_init();
    assert(err == OS_OK);

    if (main_fn) {
        err = os_task_init(&g_os_main_task, "main", os_main, main_fn,
                   OS_MAIN_TASK_PRIO,
                   (OS_MAIN_TASK_TIMER_TICKS == 0) ? OS_WAIT_FOREVER : OS_MAIN_TASK_TIMER_TICKS,
                   g_os_main_stack, OS_STACK_ALIGN(OS_MAIN_STACK_SIZE));
        assert(err == 0);
    }

    /* Call bsp related OS initializations */
    hal_bsp_init();

    err = (os_error_t) os_dev_initialize_all(OS_DEV_INIT_PRIMARY);
    assert(err == OS_OK);

    err = (os_error_t) os_dev_initialize_all(OS_DEV_INIT_SECONDARY);
    assert(err == OS_OK);
}

void
os_start(void)
{
#if MYNEWT_VAL(OS_SCHEDULING)
    os_error_t err;

    /* Enable the watchdog prior to starting the OS */
    hal_watchdog_enable();

    err = os_arch_os_start();
    assert(err == OS_OK);
#else
    assert(0);
#endif
}

void
os_reboot(int reason)
{
    sysdown(reason);
}

void
os_system_reset(void)
{
    /* Tickle watchdog just before re-entering bootloader.  Depending on what
     * the system has been doing lately, the watchdog timer might be close to
     * firing.
     */
    hal_watchdog_tickle();
    hal_system_reset();
}

void
os_pkg_init(void)
{
    os_error_t err;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    err = os_dev_initialize_all(OS_DEV_INIT_KERNEL);
    assert(err == OS_OK);

    os_mempool_module_init();
    os_msys_init();
}

/**
 *   }@ General OS functions
 * }@ OS Kernel
 */
