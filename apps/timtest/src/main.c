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
#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_timer.h"
#include "stats/stats.h"
#include "config/config.h"
#include <os/os_dev.h>
#include <assert.h>
#include <string.h>
#include "app_util_platform.h"
#include "app_error.h"

/* Init all tasks */
volatile int tasks_initialized;
int init_tasks(void);

/* Task 1 */
#define TASK1_PRIO (1)
#define TASK1_STACK_SIZE    OS_STACK_ALIGN(64)
struct os_task task1;
os_stack_t stack1[TASK1_STACK_SIZE];

#define TASK1_TIMER_NUM     (1)
#define TASK1_TIMER_FREQ    (1000000)

/* Task 2 */
#define TASK2_PRIO (2)
#define TASK2_STACK_SIZE    OS_STACK_ALIGN(64)
struct os_task task2;
os_stack_t stack2[TASK2_STACK_SIZE];

#define TASK2_TIMER_NUM     (2)
#define TASK2_TIMER_FREQ    (250000)

/* For LED toggling */
int g_led1_pin;
int g_led2_pin;
struct os_sem g_test_sem;

struct hal_timer g_task1_timer;
uint32_t task1_timer_arg = 0xdeadc0de;

void
task1_timer_cb(void *arg)
{
    uint32_t timer_arg_val;

    timer_arg_val = *(uint32_t *)arg;
    assert(timer_arg_val == 0xdeadc0de);

    os_sem_release(&g_test_sem);
}

void
task1_handler(void *arg)
{
    int rc;
    int cntr;
    uint32_t timer_cntr;

    /* Task 1 toggles LED 1 (LED_BLINK_PIN) */
    g_led1_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led1_pin, 1);

    hal_timer_set_cb(TASK1_TIMER_NUM, &g_task1_timer, task1_timer_cb,
                     &task1_timer_arg);

    cntr = 0;
    rc = hal_timer_start(&g_task1_timer, TASK1_TIMER_FREQ);
    assert(rc == 0);

    while (1) {
        /* Wait for semaphore from ISR */
        os_sem_pend(&g_test_sem, OS_TIMEOUT_NEVER);

        /* Toggle the LED */
        hal_gpio_toggle(g_led1_pin);

        ++cntr;
        if (cntr & 1) {
            timer_cntr = hal_timer_read(TASK1_TIMER_NUM);
            hal_timer_start_at(&g_task1_timer, timer_cntr + TASK1_TIMER_FREQ);
            if ((cntr % 10) == 0) {
                hal_timer_stop(&g_task1_timer);
                os_sem_release(&g_test_sem);
            }
        } else {
            hal_timer_start(&g_task1_timer, TASK1_TIMER_FREQ);
            if ((cntr % 10) == 0) {
                hal_timer_stop(&g_task1_timer);
                os_sem_release(&g_test_sem);
            }
        }
    }
}

void
task2_handler(void *arg)
{
    int32_t delta;
    uint32_t tval1;
    uint32_t tval2;

    g_led2_pin = LED_2;
    hal_gpio_init_out(g_led2_pin, 1);

    while (1) {
        /* Read timer, block for 500 msecs, make sure timer counter counts! */
        tval1 = hal_timer_read(TASK2_TIMER_NUM);
        hal_timer_delay(TASK2_TIMER_NUM, TASK2_TIMER_FREQ / 2);
        tval2 = hal_timer_read(TASK2_TIMER_NUM);
        delta = (int32_t)(tval2 - tval1);
        assert(delta > (int)(TASK2_TIMER_FREQ / 2));

        /* Toggle LED2 */
        hal_gpio_toggle(g_led2_pin);
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
int
init_tasks(void)
{
    uint32_t res;

    /* Initialize global test semaphore */
    os_sem_init(&g_test_sem, 0);

    /* Initialize timer 0 to count at 1 MHz */
    hal_timer_init(TASK1_TIMER_NUM, TASK1_TIMER_FREQ);
    res = hal_timer_get_resolution(TASK1_TIMER_NUM);
    assert(res == 1000);

    /* Initialize timer 1 to count at 250 kHz */
    hal_timer_init(TASK2_TIMER_NUM, TASK2_TIMER_FREQ);
    res = hal_timer_get_resolution(TASK2_TIMER_NUM);
    assert(res == 4000);

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

    sysinit();
    rc = init_tasks();
    os_start();

    assert(0);

    return rc;
}

