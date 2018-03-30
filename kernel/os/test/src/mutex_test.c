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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "os_test_priv.h"

#if MYNEWT_VAL(SELFTEST)
#ifdef ARCH_sim
#define MUTEX_TEST_STACK_SIZE     OS_STACK_ALIGN(1024)
#endif

struct os_task task1;
os_stack_t *stack1;

struct os_task task2;
os_stack_t *stack2;

struct os_task task3;
os_stack_t *stack3;

struct os_task task4;
os_stack_t *stack4;

struct os_mutex g_mutex1;
struct os_mutex g_mutex2;
volatile int g_mutex_test;
#endif /* MYNEWT_VAL(SELFTEST) */

volatile int g_task1_val;
volatile int g_task2_val;
volatile int g_task3_val;
volatile int g_task4_val;

/*
 * Handlers for each of the test threads are implemented here as they
 * are shared amongst multiple test cases.
 */
/**
 * mutex test basic 
 *  
 * Basic mutex tests
 * 
 * @return int 
 */
void
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

void 
mutex_test1_task1_handler(void *arg)
{
    os_error_t err;
    struct os_task *t;
    int iters;

    t = os_sched_get_current_task();
    TEST_ASSERT(t->t_func == mutex_test1_task1_handler);

    for (iters = 0; iters < 3; iters++) {
        os_time_delay(OS_TICKS_PER_SEC / 10);

        g_task1_val = 1;

        err = os_mutex_pend(&g_mutex1, OS_TICKS_PER_SEC / 10);
        TEST_ASSERT(err == OS_OK);
        TEST_ASSERT(g_task3_val == 1);

        os_time_delay(OS_TICKS_PER_SEC / 10);
    }

    os_test_restart();
}

void 
mutex_test2_task1_handler(void *arg)
{
    os_error_t err;
    struct os_task *t;
    int iters;

    t = os_sched_get_current_task();
    TEST_ASSERT(t->t_func == mutex_test2_task1_handler);

    for (iters = 0; iters < 3; iters++) {
        err = os_mutex_pend(&g_mutex1, 0);
        TEST_ASSERT(err == OS_OK, "err=%d", err);

        g_task1_val = 1;
        os_time_delay(OS_TICKS_PER_SEC / 10);

        /* 
         * Task4 should have its mutex wait flag set; at least the first time
         * through!
         */
        if (iters == 0) {
            TEST_ASSERT(task4.t_flags & OS_TASK_FLAG_MUTEX_WAIT);
        }

        if (g_mutex_test == 4) {
            os_time_delay(150);
        }

        os_mutex_release(&g_mutex1);
        os_time_delay(OS_TICKS_PER_SEC / 10);
    }

    os_test_restart();
}

void 
mutex_task2_handler(void *arg) 
{
    os_error_t err;
    struct os_task *t;

    if (g_mutex_test == 1) {
        while (1) {
            t = os_sched_get_current_task();
            TEST_ASSERT(t->t_func == mutex_task2_handler);

            os_time_delay(OS_TICKS_PER_SEC / 20);
            while (1) {
                /* Wait here forever */
            }
        }
    } else {
        if (g_mutex_test == 2) {
            /* Sleep for 500 miliseconds */
            os_time_delay(OS_TICKS_PER_SEC / 2);
        } else if (g_mutex_test == 3) {
            /* Sleep for 30 miliseconds */
            os_time_delay(OS_TICKS_PER_SEC / 33);
        }

        while (1) {
            t = os_sched_get_current_task();
            TEST_ASSERT(t->t_func == mutex_task2_handler);

            err = os_mutex_pend(&g_mutex1, OS_TICKS_PER_SEC * 10);
            if (g_mutex_test == 4) {
                TEST_ASSERT(err == OS_TIMEOUT);
            } else {
                TEST_ASSERT(err == OS_OK);
            }

            os_time_delay(OS_TICKS_PER_SEC / 10);
        }
    }
}

void 
mutex_task3_handler(void *arg) 
{
    os_error_t err;
    struct os_task *t;

    if (g_mutex_test == 1) {
        while (1) {
            t = os_sched_get_current_task();
            TEST_ASSERT(t->t_func == mutex_task3_handler);

            /* Get mutex 1 */
            err = os_mutex_pend(&g_mutex1, OS_TIMEOUT_NEVER);
            TEST_ASSERT(err == OS_OK);

            while (g_task1_val != 1) {
                /* Wait till task 1 wakes up and sets val. */
            }

            g_task3_val = 1;

            err = os_mutex_release(&g_mutex1);
            TEST_ASSERT(err == OS_OK);
        }
    } else {
        if (g_mutex_test == 2) {
            /* Sleep for 3 seconds */
            t = os_sched_get_current_task();
            os_time_delay(OS_TICKS_PER_SEC / 33);
        } else if (g_mutex_test == 3) {
            /* Sleep for 3 seconds */
            t = os_sched_get_current_task();
            os_time_delay(OS_TICKS_PER_SEC / 20);
        }

        while (1) {
            t = os_sched_get_current_task();
            TEST_ASSERT(t->t_func == mutex_task3_handler);

            err = os_mutex_pend(&g_mutex1, OS_TICKS_PER_SEC * 10);
            if (g_mutex_test == 4) {
                TEST_ASSERT(err == OS_TIMEOUT);
            } else {
                TEST_ASSERT(err == OS_OK);
            }

            if (err == OS_OK) {
                err = os_mutex_release(&g_mutex1);
                TEST_ASSERT(err == OS_OK);
            }

            os_time_delay(OS_TICKS_PER_SEC * 10);
        }
    }
}

void
mutex_task4_handler(void *arg)
{
    os_error_t err;
    struct os_task *t;

    while (1) {
        t = os_sched_get_current_task();
        TEST_ASSERT(t->t_func == mutex_task4_handler);

        if (g_mutex_test == 5) {
            err = os_mutex_pend(&g_mutex1, OS_TICKS_PER_SEC / 10);
        } else {
            err = os_mutex_pend(&g_mutex1, OS_TICKS_PER_SEC * 10);
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

        os_time_delay(OS_TICKS_PER_SEC * 10);
    }
}

void
os_mutex_tc_pretest(void* arg)
{
#if MYNEWT_VAL(SELFTEST)
    /*
     * Only call if running in "native" simulated environment
     */
    os_init(NULL);
    sysinit();
#endif
    return;
}

void
os_mutex_tc_posttest(void* arg)
{
#if MYNEWT_VAL(SELFTEST)
    /*
     * Only call if running in "native" simulated environment
     */
    os_start();
#endif
    return;
}

void
os_mutex_test_init(void *arg)
{

    /*
     * Stack should be allocated in target environemnt
     * so they are sized correctly.
     */
#if MYNEWT_VAL(SELFTEST)
    stack1 = malloc(sizeof(os_stack_t) * MUTEX_TEST_STACK_SIZE);
    assert(stack1);
    stack1_size = MUTEX_TEST_STACK_SIZE;
    stack2 = malloc(sizeof(os_stack_t) * MUTEX_TEST_STACK_SIZE);
    assert(stack2);
    stack2_size = MUTEX_TEST_STACK_SIZE;
    stack3 = malloc(sizeof(os_stack_t) * MUTEX_TEST_STACK_SIZE);
    assert(stack3);
    stack3_size = MUTEX_TEST_STACK_SIZE;
    stack4 = malloc(sizeof(os_stack_t) * MUTEX_TEST_STACK_SIZE);
    assert(stack4);
    stack4_size = MUTEX_TEST_STACK_SIZE;
#endif
}

TEST_CASE_DECL(os_mutex_test_basic)
TEST_CASE_DECL(os_mutex_test_case_1)
TEST_CASE_DECL(os_mutex_test_case_2)

TEST_SUITE(os_mutex_test_suite)
{
    tu_case_set_post_cb(os_mutex_tc_posttest, NULL);
    os_mutex_test_basic();

    tu_case_set_pre_cb(os_mutex_tc_pretest, NULL);
    tu_case_set_post_cb(os_mutex_tc_posttest, NULL);
    os_mutex_test_case_1();

    tu_case_set_pre_cb(os_mutex_tc_pretest, NULL);
    tu_case_set_post_cb(os_mutex_tc_posttest, NULL);
    os_mutex_test_case_2();
}
