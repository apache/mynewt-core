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
#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "os_test_priv.h"

struct os_sem g_sem1;

/*
 * TEST NUMBERS:
 *  10: In this test we have the highest priority task getting the semaphore
 *  then sleeping. Two lower priority tasks then wake up and attempt to get
 *  the semaphore. They are blocked until the higher priority task releases
 *  the semaphore, at which point the lower priority tasks should wake up in
 *  order, get the semaphore, then release it and go back to sleep.
 *
 */
char sem_test_buf[128];

/**
 * sem test disp sem
 *
 * Display semaphore contents
 *
 * @param sem
 */
const char *
sem_test_sem_to_s(const struct os_sem *sem)
{
    snprintf(sem_test_buf, sizeof sem_test_buf, "\tSemaphore: tokens=%u head=%p",
             sem->sem_tokens, SLIST_FIRST(&sem->sem_head));

    return sem_test_buf;
}

void
sem_test_sleep_task_handler(void *arg)
{
    struct os_task *t;

    t = os_sched_get_current_task();
    TEST_ASSERT(t->t_func == sem_test_sleep_task_handler);

    os_time_delay(2 * OS_TICKS_PER_SEC);
    os_test_restart();
}

void
sem_test_pend_release_loop(int delay, int timeout, int itvl)
{
    os_error_t err;

    os_time_delay(delay);

    while (1) {
        err = os_sem_pend(&g_sem1, timeout);
        TEST_ASSERT((err == OS_OK) || (err == OS_TIMEOUT));

        err = os_sem_release(&g_sem1);
        TEST_ASSERT(err == OS_OK);

        os_time_delay(itvl);
    }
}

/**
 * sem test basic
 *
 * Basic semaphore tests
 *
 * @return int
 */
void
sem_test_basic_handler(void *arg)
{
    struct os_task *t;
    struct os_sem *sem;
    os_error_t err;

    /* Prevent unused-but-set-variable warning when system assert is
     * enabled.
     */
    (void)t;

    sem = &g_sem1;
    t = os_sched_get_current_task();

    /* Test some error cases */
    TEST_ASSERT(os_sem_init(NULL, 1)    == OS_INVALID_PARM);
    TEST_ASSERT(os_sem_release(NULL)    == OS_INVALID_PARM);
    TEST_ASSERT(os_sem_pend(NULL, 1)    == OS_INVALID_PARM);

    /* Get the semaphore */
    err = os_sem_pend(sem, 0);
    TEST_ASSERT(err == 0,
                "Did not get free semaphore immediately (err=%d)", err);

    /* Check semaphore internals */
    TEST_ASSERT(sem->sem_tokens == 0 && SLIST_EMPTY(&sem->sem_head),
                "Semaphore internals wrong after getting semaphore\n"
                "%s\n"
                "Task: task=%p prio=%u", sem_test_sem_to_s(sem),
                t, t->t_prio);

    /* Get the semaphore again; should fail */
    err = os_sem_pend(sem, 0);
    TEST_ASSERT(err == OS_TIMEOUT,
                "Did not time out waiting for semaphore (err=%d)", err);

    /* Check semaphore internals */
    TEST_ASSERT(sem->sem_tokens == 0 && SLIST_EMPTY(&sem->sem_head),
                "Semaphore internals wrong after getting semaphore\n"
                "%s\n"
                "Task: task=%p prio=%u\n", sem_test_sem_to_s(sem), t,
                t->t_prio);

    /* Release semaphore */
    err = os_sem_release(sem);
    TEST_ASSERT(err == 0,
                "Could not release semaphore I own (err=%d)", err);

    /* Check semaphore internals */
    TEST_ASSERT(sem->sem_tokens == 1 && SLIST_EMPTY(&sem->sem_head),
                "Semaphore internals wrong after releasing semaphore\n"
                "%s\n"
                "Task: task=%p prio=%u\n", sem_test_sem_to_s(sem), t,
                t->t_prio);

    /* Release it again */
    err = os_sem_release(sem);
    TEST_ASSERT(err == 0,
                "Could not release semaphore again (err=%d)\n", err);

    /* Check semaphore internals */
    TEST_ASSERT(sem->sem_tokens == 2 && SLIST_EMPTY(&sem->sem_head),
                "Semaphore internals wrong after releasing semaphore\n"
                "%s\n"
                "Task: task=%p prio=%u\n", sem_test_sem_to_s(sem), t,
                t->t_prio);

    os_test_restart();
}

void
sem_test_1_task1_handler(void *arg)
{
    os_error_t err;
    struct os_task *t;
    int i;;

    for (i = 0; i < 3; i++) {
        t = os_sched_get_current_task();
        TEST_ASSERT(t->t_func == sem_test_1_task1_handler);

        err = os_sem_pend(&g_sem1, 0);
        TEST_ASSERT(err == OS_OK);

        /* Sleep to let other tasks run */
        os_time_delay(OS_TICKS_PER_SEC / 10);

        /* Release the semaphore */
        err = os_sem_release(&g_sem1);
        TEST_ASSERT(err == OS_OK);

        /* Sleep to let other tasks run */
        os_time_delay(OS_TICKS_PER_SEC / 10);
    }

    os_test_restart();
}

void
sem_test_1_task2_handler(void *arg)
{
    sem_test_pend_release_loop(0, OS_TICKS_PER_SEC / 10,
                               OS_TICKS_PER_SEC / 10);
}

void
sem_test_1_task3_handler(void *arg)
{
    sem_test_pend_release_loop(0, OS_TIMEOUT_NEVER, OS_TICKS_PER_SEC * 2);
}

void
sem_test_2_task2_handler(void *arg)
{
    sem_test_pend_release_loop(0, 2000, 2000);
}

void
sem_test_2_task3_handler(void *arg)
{
    sem_test_pend_release_loop(0, OS_TIMEOUT_NEVER, 2000);
}

void
sem_test_2_task4_handler(void *arg)
{
    sem_test_pend_release_loop(0, 2000, 2000);
}

void
sem_test_3_task2_handler(void *arg)
{
    sem_test_pend_release_loop(100, 2000, 2000);
}

void
sem_test_3_task3_handler(void *arg)
{
    sem_test_pend_release_loop(150, 2000, 2000);
}

void
sem_test_3_task4_handler(void *arg)
{
    sem_test_pend_release_loop(0, 2000, 2000);
}

void
sem_test_4_task2_handler(void *arg)
{
    sem_test_pend_release_loop(60, 2000, 2000);
}

void
sem_test_4_task3_handler(void *arg)
{
    sem_test_pend_release_loop(60, 2000, 2000);
}

void
sem_test_4_task4_handler(void *arg)
{
    sem_test_pend_release_loop(0, 2000, 2000);
}

TEST_CASE_DECL(os_sem_test_basic)
TEST_CASE_DECL(os_sem_test_case_1)
TEST_CASE_DECL(os_sem_test_case_2)
TEST_CASE_DECL(os_sem_test_case_3)
TEST_CASE_DECL(os_sem_test_case_4)

TEST_SUITE(os_sem_test_suite)
{
    os_sem_test_basic();
    os_sem_test_case_1();
    os_sem_test_case_2();
    os_sem_test_case_3();
    os_sem_test_case_4();
}
