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
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "console/console.h"
#include "shell/shell.h"
#include "log/log.h"
#include "stats/stats.h"
#include "config/config.h"
#include <assert.h>
#include <string.h>
#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif

/* Init all tasks */
volatile int tasks_initialized;
int init_tasks(void);

/* Task 1 */
#define TASK1_PRIO (1)
#define TASK1_STACK_SIZE    OS_STACK_ALIGN(1024)
struct os_task task1;
os_stack_t stack1[TASK1_STACK_SIZE];
static volatile int g_task1_loops;

/* Task 2 */
#define TASK2_PRIO (2)
#define TASK2_STACK_SIZE    OS_STACK_ALIGN(1024)
struct os_task task2;
os_stack_t stack2[TASK2_STACK_SIZE];

#define SHELL_TASK_PRIO (3)
#define SHELL_MAX_INPUT_LEN     (256)
#define SHELL_TASK_STACK_SIZE (OS_STACK_ALIGN(1024))
os_stack_t shell_stack[SHELL_TASK_STACK_SIZE];

struct log_handler log_console_handler;
struct log my_log;

static volatile int g_task2_loops;

/* Global test semaphore */
struct os_sem g_test_sem;

/* For LED toggling */
int g_led_pin;

#define DEFAULT_MBUF_MPOOL_BUF_LEN (256)
#define DEFAULT_MBUF_MPOOL_NBUFS (5)

uint8_t default_mbuf_mpool_data[DEFAULT_MBUF_MPOOL_BUF_LEN *
    DEFAULT_MBUF_MPOOL_NBUFS];

struct os_mbuf_pool default_mbuf_pool;
struct os_mempool default_mbuf_mpool;


void
task1_handler(void *arg)
{
    struct os_task *t;

    /* Set the led pin for the E407 devboard */
    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);

    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task1_handler);

        ++g_task1_loops;

        /* Wait one second */
        os_time_delay(OS_TICKS_PER_SEC);

        /* Toggle the LED */
        hal_gpio_toggle(g_led_pin);

        /* Release semaphore to task 2 */
        os_sem_release(&g_test_sem);
    }
}

void
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
 * Called by main.c after os_init(). This function performs initializations
 * that are required before tasks are running.
 *
 * @return int 0 success; error otherwise.
 */
int
init_tasks(void)
{
    /* Initialize global test semaphore */
    os_sem_init(&g_test_sem, 0);

    os_task_init(&task1, "task1", task1_handler, NULL,
            TASK1_PRIO, OS_WAIT_FOREVER, stack1, TASK1_STACK_SIZE);

    os_task_init(&task2, "task2", task2_handler, NULL,
            TASK2_PRIO, OS_WAIT_FOREVER, stack2, TASK2_STACK_SIZE);

    tasks_initialized = 1;
    return 0;
}

/**
 * main
 *
 * The main function for the project. This function initializes the os, calls
 * init_tasks to initialize tasks (and possibly other objects), then starts the
 * OS. We should not return from os start.
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

    conf_init();

    os_init();

    rc = os_mempool_init(&default_mbuf_mpool, DEFAULT_MBUF_MPOOL_NBUFS,
            DEFAULT_MBUF_MPOOL_BUF_LEN, default_mbuf_mpool_data,
            "default_mbuf_data");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&default_mbuf_pool, &default_mbuf_mpool,
            DEFAULT_MBUF_MPOOL_BUF_LEN, DEFAULT_MBUF_MPOOL_NBUFS);
    assert(rc == 0);

    rc = os_msys_register(&default_mbuf_pool);
    assert(rc == 0);

    shell_task_init(SHELL_TASK_PRIO, shell_stack, SHELL_TASK_STACK_SIZE,
                    SHELL_MAX_INPUT_LEN);

    stats_module_init();

    rc = init_tasks();
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return rc;
}

