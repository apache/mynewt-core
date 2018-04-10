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
#include <string.h>
#include "os/mynewt.h"
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash.h>
#include <console/console.h>
#include <shell/shell.h>
#include <log/log.h>
#include <stats/stats.h>
#include <config/config.h>
#include <flash_map/flash_map.h>
#include <hal/hal_system.h>
#include <newtmgr/newtmgr.h>
#include <bootutil/image.h>
#include <bootutil/bootutil.h>
#include <imgmgr/imgmgr.h>
#include <reboot/log_reboot.h>
#include <id/id.h>

#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif

/* Task 1 */
#define TASK1_PRIO (8)
#define TASK1_STACK_SIZE    OS_STACK_ALIGN(128)
#define MAX_CBMEM_BUF 300
static struct os_task task1;
static volatile int g_task1_loops;

/* Task 2 */
#define TASK2_PRIO (9)
#define TASK2_STACK_SIZE    OS_STACK_ALIGN(32)
static struct os_task task2;

static struct log my_log;

static volatile int g_task2_loops;

/* Global test semaphore */
static struct os_sem g_test_sem;

/* For LED toggling */
static int g_led_pin;

STATS_SECT_START(gpio_stats)
STATS_SECT_ENTRY(toggles)
STATS_SECT_END

static STATS_SECT_DECL(gpio_stats) g_stats_gpio_toggle;

STATS_NAME_START(gpio_stats)
STATS_NAME(gpio_stats, toggles)
STATS_NAME_END(gpio_stats)

static uint32_t cbmem_buf[MAX_CBMEM_BUF];
static struct cbmem cbmem;

static void
task1_handler(void *arg)
{
    struct os_task *t;
    int prev_pin_state, curr_pin_state;
    struct image_version ver;

    /* Set the led pin for the E407 devboard */
    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);

    if (imgr_my_version(&ver) == 0) {
        console_printf("\nSplitty %u.%u.%u.%u\n",
          ver.iv_major, ver.iv_minor, ver.iv_revision,
          (unsigned int)ver.iv_build_num);
    } else {
        console_printf("\nSplitty\n");
    }

    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task1_handler);

        ++g_task1_loops;

        /* Wait one second */
        os_time_delay(OS_TICKS_PER_SEC/4);

        /* Toggle the LED */
        prev_pin_state = hal_gpio_read(g_led_pin);
        curr_pin_state = hal_gpio_toggle(g_led_pin);
        LOG_INFO(&my_log, LOG_MODULE_DEFAULT, "GPIO toggle from %u to %u",
            prev_pin_state, curr_pin_state);
        STATS_INC(g_stats_gpio_toggle, toggles);

        /* Release semaphore to task 2 */
        os_sem_release(&g_test_sem);
    }
}

static void
task2_handler(void *arg)
{
    struct os_task *t;

    while (1) {
        /* just for debug; task 2 should be the running task */
        t = os_sched_get_current_task();
        assert(t->t_func == task2_handler);

        /* Increment # of times we went through task loop */
        ++g_task2_loops;

        /* Wait for semaphore from ISR */
        os_sem_pend(&g_test_sem, OS_TIMEOUT_NEVER);
    }
}

/**
 * init_tasks
 *
 * Called by main.c after sysinit(). This function performs initializations
 * that are required before tasks are running.
 *
 * @return int 0 success; error otherwise.
 */
static void
init_tasks(void)
{
    os_stack_t *pstack;

    /* Initialize global test semaphore */
    os_sem_init(&g_test_sem, 0);

    pstack = malloc(sizeof(os_stack_t)*TASK1_STACK_SIZE);
    assert(pstack);

    os_task_init(&task1, "task1", task1_handler, NULL,
            TASK1_PRIO, OS_WAIT_FOREVER, pstack, TASK1_STACK_SIZE);

    pstack = malloc(sizeof(os_stack_t)*TASK2_STACK_SIZE);
    assert(pstack);

    os_task_init(&task2, "task2", task2_handler, NULL,
            TASK2_PRIO, OS_WAIT_FOREVER, pstack, TASK2_STACK_SIZE);

}

/**
 * main
 *
 * The main task for the project. This function initializes the packages, calls
 * init_tasks to initialize additional tasks (and possibly other objects),
 * then starts serving events from default event queue.
 *
 * @return int NOTE: this function should never return!
 */
int
main(int argc, char **argv)
{
    int rc;

#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif

    sysinit();

    cbmem_init(&cbmem, cbmem_buf, MAX_CBMEM_BUF);
    log_register("log", &my_log, &log_cbmem_handler, &cbmem, LOG_SYSLEVEL);

    stats_init(STATS_HDR(g_stats_gpio_toggle),
               STATS_SIZE_INIT_PARMS(g_stats_gpio_toggle, STATS_SIZE_32),
               STATS_NAME_INIT_PARMS(gpio_stats));

    stats_register("gpio_toggle", STATS_HDR(g_stats_gpio_toggle));

    conf_load();

    reboot_start(hal_reset_cause());

    init_tasks();

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    /* Never exit */
    return rc;
}
