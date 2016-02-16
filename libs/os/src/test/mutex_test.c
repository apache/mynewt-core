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
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include "testutil/testutil.h"
#include "os/os.h"
#include "os/os_test.h"
#include "os/os_cfg.h"
#include "os/os_mutex.h"
#include "os_test_priv.h"

#ifdef ARCH_sim
#define MUTEX_TEST_STACK_SIZE   1024
#else
#define MUTEX_TEST_STACK_SIZE   256
#endif

struct os_task task14;
os_stack_t stack14[OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE)];

struct os_task task15;
os_stack_t stack15[OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE)];

struct os_task task16;
os_stack_t stack16[OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE)];

struct os_task task17;
os_stack_t stack17[OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE)];

#define TASK14_PRIO (4)
#define TASK15_PRIO (5)
#define TASK16_PRIO (6)
#define TASK17_PRIO (7)

volatile int g_task14_val;
volatile int g_task15_val;
volatile int g_task16_val;
volatile int g_task17_val;
struct os_mutex g_mutex1;
struct os_mutex g_mutex2;

static volatile int g_mutex_test;

/**
 * mutex test basic 
 *  
 * Basic mutex tests
 * 
 * @return int 
 */
static void
mutex_test_basic_handler(void *arg)
{
    struct os_mutex *mu;
    struct os_task *t;
    os_error_t err;

    mu = &g_mutex1;
    t = os_sched_get_current_task();

    /* Test some error cases */
    TEST_ASSERT(os_mutex_init(NULL)     == OS_INVALID_PARM);
    TEST_ASSERT(os_mutex_release(NULL)  == OS_INVALID_PARM);
    TEST_ASSERT(os_mutex_pend(NULL, 0)  == OS_INVALID_PARM);

    /* Get the mutex */
    err = os_mutex_pend(mu, 0);
    TEST_ASSERT(err == 0,
                "Did not get free mutex immediately (err=%d)", err);

    /* Check mutex internals */
    TEST_ASSERT(mu->mu_owner == t && mu->mu_level == 1 &&
                mu->mu_prio == t->t_prio && SLIST_EMPTY(&mu->mu_head),
                "Mutex internals not correct after getting mutex\n"
                "Mutex: owner=%p prio=%u level=%u head=%p\n"
                "Task: task=%p prio=%u",
                mu->mu_owner, mu->mu_prio, mu->mu_level, 
                SLIST_FIRST(&mu->mu_head),
                t, t->t_prio);

    /* Get the mutex again; should be level 2 */
    err = os_mutex_pend(mu, 0);
    TEST_ASSERT(err == 0, "Did not get my mutex immediately (err=%d)", err);

    /* Check mutex internals */
    TEST_ASSERT(mu->mu_owner == t && mu->mu_level == 2 &&
                mu->mu_prio == t->t_prio && SLIST_EMPTY(&mu->mu_head),
                "Mutex internals not correct after getting mutex\n"
                "Mutex: owner=%p prio=%u level=%u head=%p\n"
                "Task: task=%p prio=%u",
                mu->mu_owner, mu->mu_prio, mu->mu_level, 
                SLIST_FIRST(&mu->mu_head), t, t->t_prio);

    /* Release mutex */
    err = os_mutex_release(mu);
    TEST_ASSERT(err == 0, "Could not release mutex I own (err=%d)", err);

    /* Check mutex internals */
    TEST_ASSERT(mu->mu_owner == t && mu->mu_level == 1 &&
                mu->mu_prio == t->t_prio && SLIST_EMPTY(&mu->mu_head),
                "Error: mutex internals not correct after getting mutex\n"
                "Mutex: owner=%p prio=%u level=%u head=%p\n"
                "Task: task=%p prio=%u",
                mu->mu_owner, mu->mu_prio, mu->mu_level, 
                SLIST_FIRST(&mu->mu_head), t, t->t_prio);

    /* Release it again */
    err = os_mutex_release(mu);
    TEST_ASSERT(err == 0, "Could not release mutex I own (err=%d)", err);

    /* Check mutex internals */
    TEST_ASSERT(mu->mu_owner == NULL && mu->mu_level == 0 &&
                mu->mu_prio == t->t_prio && SLIST_EMPTY(&mu->mu_head),
                "Mutex internals not correct after getting mutex\n"
                "Mutex: owner=%p prio=%u level=%u head=%p\n"
                "Task: task=%p prio=%u",
                mu->mu_owner, mu->mu_prio, mu->mu_level, 
                SLIST_FIRST(&mu->mu_head), t, t->t_prio);

    os_test_restart();
}

static void 
mutex_test1_task14_handler(void *arg)
{
    os_error_t err;
    struct os_task *t;
    int iters;

    t = os_sched_get_current_task();
    TEST_ASSERT(t->t_func == mutex_test1_task14_handler);

    for (iters = 0; iters < 3; iters++) {
        os_time_delay(100);

        g_task14_val = 1;

        err = os_mutex_pend(&g_mutex1, 100);
        TEST_ASSERT(err == OS_OK);
        TEST_ASSERT(g_task16_val == 1);

        os_time_delay(100);
    }

    os_test_restart();
}

static void 
mutex_test2_task14_handler(void *arg)
{
    os_error_t err;
    struct os_task *t;
    int iters;

    t = os_sched_get_current_task();
    TEST_ASSERT(t->t_func == mutex_test2_task14_handler);

    for (iters = 0; iters < 3; iters++) {
        err = os_mutex_pend(&g_mutex1, 0);
        TEST_ASSERT(err == OS_OK, "err=%d", err);

        g_task14_val = 1;
        os_time_delay(100);

        /* 
         * Task17 should have its mutex wait flag set; at least the first time
         * through!
         */
        if (iters == 0) {
            TEST_ASSERT(task17.t_flags & OS_TASK_FLAG_MUTEX_WAIT);
        }

        if (g_mutex_test == 4) {
            os_time_delay(150);
        }

        os_mutex_release(&g_mutex1);
        os_time_delay(100);
    }

    os_test_restart();
}

static void 
task15_handler(void *arg) 
{
    os_error_t err;
    struct os_task *t;

    if (g_mutex_test == 1) {
        while (1) {
            t = os_sched_get_current_task();
            TEST_ASSERT(t->t_func == task15_handler);

            os_time_delay(50);
            while (1) {
                /* Wait here forever */
            }
        }
    } else {
        if (g_mutex_test == 2) {
            /* Sleep for 3 seconds */
            t = os_sched_get_current_task();
            os_time_delay(500);
        } else if (g_mutex_test == 3) {
            /* Sleep for 3 seconds */
            t = os_sched_get_current_task();
            os_time_delay(30);
        }

        while (1) {
            t = os_sched_get_current_task();
            TEST_ASSERT(t->t_func == task15_handler);

            err = os_mutex_pend(&g_mutex1, 10000);
            if (g_mutex_test == 4) {
                TEST_ASSERT(err == OS_TIMEOUT);
            } else {
                TEST_ASSERT(err == OS_OK);
            }

            os_time_delay(100);
        }
    }
}

static void 
task16_handler(void *arg) 
{
    os_error_t err;
    struct os_task *t;

    if (g_mutex_test == 1) {
        while (1) {
            t = os_sched_get_current_task();
            TEST_ASSERT(t->t_func == task16_handler);

            /* Get mutex 1 */
            err = os_mutex_pend(&g_mutex1, 0xFFFFFFFF);
            TEST_ASSERT(err == OS_OK);

            while (g_task14_val != 1) {
                /* Wait till task 1 wakes up and sets val. */
            }

            g_task16_val = 1;

            err = os_mutex_release(&g_mutex1);
            TEST_ASSERT(err == OS_OK);
        }
    } else {
        if (g_mutex_test == 2) {
            /* Sleep for 3 seconds */
            t = os_sched_get_current_task();
            os_time_delay(30);
        } else if (g_mutex_test == 3) {
            /* Sleep for 3 seconds */
            t = os_sched_get_current_task();
            os_time_delay(50);
        }

        while (1) {
            t = os_sched_get_current_task();
            TEST_ASSERT(t->t_func == task16_handler);

            err = os_mutex_pend(&g_mutex1, 10000);
            if (g_mutex_test == 4) {
                TEST_ASSERT(err == OS_TIMEOUT);
            } else {
                TEST_ASSERT(err == OS_OK);
            }

            if (err == OS_OK) {
                err = os_mutex_release(&g_mutex1);
                TEST_ASSERT(err == OS_OK);
            }

            os_time_delay(10000);
        }
    }
}

static void 
task17_handler(void *arg)
{
    os_error_t err;
    struct os_task *t;

    while (1) {
        t = os_sched_get_current_task();
        TEST_ASSERT(t->t_func == task17_handler);

        if (g_mutex_test == 5) {
            err = os_mutex_pend(&g_mutex1, 10);
        } else {
            err = os_mutex_pend(&g_mutex1, 10000);
            TEST_ASSERT((t->t_flags & OS_TASK_FLAG_MUTEX_WAIT) == 0);
        }

        if (g_mutex_test == 4 || g_mutex_test == 5) {
            TEST_ASSERT(err == OS_TIMEOUT);
        } else {
            TEST_ASSERT(err == OS_OK);
        }

        if (err == OS_OK) {
            err = os_mutex_release(&g_mutex1);
            TEST_ASSERT(err == OS_OK);
        }

        os_time_delay(10000);
    }
}

TEST_CASE(os_mutex_test_basic)
{
    os_init();

    os_mutex_init(&g_mutex1);

    os_task_init(&task14, "task14", mutex_test_basic_handler, NULL,
                 TASK14_PRIO, OS_WAIT_FOREVER, stack14,
                 OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE));

    os_start();
}

TEST_CASE(os_mutex_test_case_1)
{
    int rc;

    os_init();

    g_mutex_test = 1;
    g_task14_val = 0;
    g_task15_val = 0;
    g_task16_val = 0;

    rc = os_mutex_init(&g_mutex1);
    TEST_ASSERT(rc == 0);
    rc = os_mutex_init(&g_mutex2);
    TEST_ASSERT(rc == 0);

    os_task_init(&task14, "task14", mutex_test1_task14_handler, NULL,
                 TASK14_PRIO, OS_WAIT_FOREVER, stack14,
                 OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE));

    os_task_init(&task15, "task15", task15_handler, NULL, TASK15_PRIO, 
            OS_WAIT_FOREVER, stack15, OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE));

    os_task_init(&task16, "task16", task16_handler, NULL, TASK16_PRIO, 
            OS_WAIT_FOREVER, stack16, OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE));

    os_start();
}

TEST_CASE(os_mutex_test_case_2)
{
    os_init();

    g_mutex_test = 2;
    g_task14_val = 0;
    g_task15_val = 0;
    g_task16_val = 0;
    os_mutex_init(&g_mutex1);
    os_mutex_init(&g_mutex2);

    os_task_init(&task14, "task14", mutex_test2_task14_handler, NULL,
                 TASK14_PRIO, OS_WAIT_FOREVER, stack14,
                 OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE));

    os_task_init(&task15, "task15", task15_handler, NULL, TASK15_PRIO, 
            OS_WAIT_FOREVER, stack15, OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE));

    os_task_init(&task16, "task16", task16_handler, NULL, TASK16_PRIO, 
            OS_WAIT_FOREVER, stack16, OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE));

    os_task_init(&task17, "task17", task17_handler, NULL, TASK17_PRIO, 
            OS_WAIT_FOREVER, stack17, OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE));
 
    os_start();
}

TEST_SUITE(os_mutex_test_suite)
{
    os_mutex_test_basic();
    os_mutex_test_case_1();
    os_mutex_test_case_2();
}
