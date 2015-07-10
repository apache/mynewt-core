/**
 * Copyright (c) 2015 Stack Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "os/os.h"
#include "os/os_cfg.h"
#include "os/os_sem.h"

struct os_task task10;
os_stack_t stack10[OS_STACK_ALIGN(1024)]; 

struct os_task task11;
os_stack_t stack11[OS_STACK_ALIGN(1024)];

struct os_task task12;
os_stack_t stack12[OS_STACK_ALIGN(1024)];

struct os_task task13;
os_stack_t stack13[OS_STACK_ALIGN(1024)];

#define TASK10_PRIO (10) 
#define TASK11_PRIO (11) 
#define TASK12_PRIO (12) 
#define TASK13_PRIO (13) 

volatile int g_task10_val;
volatile int g_task11_val;
volatile int g_task12_val;
volatile int g_task13_val;
volatile int g_task11_print;
struct os_sem g_sem1;
struct os_sem g_sem2;

static volatile int g_sem_test;

/* 
 * TEST NUMBERS:
 *      10: XXX
 * 
 */

/**
 * sem test disp sem
 *  
 * Display semaphore contents 
 * 
 * @param sem 
 */
void
sem_test_disp_sem(struct os_sem *sem)
{
    printf ("\tSemaphore: tokens=%u head=%p\n", sem->sem_tokens,
            SLIST_FIRST(&sem->sem_head));
    fflush(stdout);
}

/**
 * sem test basic 
 *  
 * Basic semaphore tests
 * 
 * @return int 
 */
static int 
sem_test_basic(struct os_sem *sem, struct os_task *t)
{
    os_error_t err;

    /* Test errors first */
    printf("Performing basic semaphore testing\n");

    /* Test some error cases */
    err = os_sem_create(NULL, 1);
    if (err != OS_INVALID_PARM) {
        printf("Error: expected error from semaphore create (err=%d)\n", err);
        goto semaphore_err_exit;
    }
    err = os_sem_delete(NULL);
    if (err != OS_INVALID_PARM) {
        printf("Error: expected error from semaphore delete (err=%d)\n", err);
        goto semaphore_err_exit;
    }
    err = os_sem_release(NULL);
    if (err != OS_INVALID_PARM) {
        printf("Error: expected error from semaphore release (err=%d)\n", err);
        goto semaphore_err_exit;
    }
    err = os_sem_pend(NULL, 1);
    if (err != OS_INVALID_PARM) {
        printf("Error: expected error from semaphore pend (err=%d)\n", err);
        goto semaphore_err_exit;
    }

    /* Get the semaphore */
    err = os_sem_pend(sem, 0);
    if (err) {
        printf("Error: did not get free semaphore immediately (err=%d)\n", err);
    }

    /* Check semaphore internals */
    if ((sem->sem_tokens != 0) || !SLIST_EMPTY(&sem->sem_head)) {
        printf("Error: semaphore internals wrong after getting semaphore\n");
        sem_test_disp_sem(sem);
        printf("Task: task=%p prio=%u\n", t, t->t_prio);
        goto semaphore_err_exit;
    }

    /* Get the semaphore again; should fail */
    err = os_sem_pend(sem, 0);
    if (err != OS_TIMEOUT) {
        printf("Error: Did not time out waiting for semaphore (err=%d)\n", err);
    }

    /* Check semaphore internals */
    if ((sem->sem_tokens != 0) || !SLIST_EMPTY(&sem->sem_head)) {
        printf("Error: semaphore internals wrong after getting semaphore\n");
        sem_test_disp_sem(sem);
        printf("Task: task=%p prio=%u\n", t, t->t_prio);
        goto semaphore_err_exit;
    }

    /* Release semaphore */
    err = os_sem_release(sem);
    if (err) {
        printf("Error: could not release semaphore I own (err=%d)\n", err);
        goto semaphore_err_exit;
    }

    /* Check semaphore internals */
    if ((sem->sem_tokens != 1) || !SLIST_EMPTY(&sem->sem_head)) {
        printf("Error: semaphore internals wrong after releasing semaphore\n");
        sem_test_disp_sem(sem);
        printf("Task: task=%p prio=%u\n", t, t->t_prio);
        goto semaphore_err_exit;
    }

    /* Release it again */
    err = os_sem_release(sem);
    if (err) {
        printf("Error: could not release semaphore again (err=%d)\n", err);
        goto semaphore_err_exit;
    }

    /* Check semaphore internals */
    if ((sem->sem_tokens != 2) || !SLIST_EMPTY(&sem->sem_head)) {
        printf("Error: semaphore internals wrong after releasing semaphore\n");
        sem_test_disp_sem(sem);
        printf("Task: task=%p prio=%u\n", t, t->t_prio);
        goto semaphore_err_exit;
    }

    /* "Delete" it */
    err = os_sem_delete(sem);
    if (err) {
        printf("Error: could not delete semaphore (err=%d)\n", err);
        goto semaphore_err_exit;
    }

    /* Check semaphore internals */
    if ((sem->sem_tokens != 0) || !SLIST_EMPTY(&sem->sem_head)) {
        printf("Error: semaphore internals wrong after deleting semaphore\n");
        sem_test_disp_sem(sem);
        printf("Task: task=%p prio=%u\n", t, t->t_prio);
        goto semaphore_err_exit;
    }

    printf("Finished basic semaphore testing.\n");

    return 0;

semaphore_err_exit:
    return -1;
}

void 
task10_handler(void *arg)
{
    os_error_t err;
    struct os_task *t;

    /* Do some basic semaphore testing first */
    if (sem_test_basic(&g_sem1, os_sched_get_current_task())) {
        exit(0);
    }

    /* Must "re-create" sem1 for testing */
    err = os_sem_create(&g_sem1, 1);
    assert(err == OS_OK);

    if (g_sem_test == 10) {
        while (1) {
            t = os_sched_get_current_task();
            assert(t->t_func == task10_handler);

            printf("Task 10 Loop (ostime=%u)\n", os_time_get());
            g_task10_val = 1;

            err = os_sem_pend(&g_sem1, 0);
            assert(err == OS_OK);
            printf("Task 10 gets semaphore 1 (ostime=%u)\n", os_time_get());
            fflush(stdout);
            sem_test_disp_sem(&g_sem1);

            /* Sleep to let other tasks run */
            printf("Task 10 sleeping for 5 seconds (ostime=%u)\n", 
                   os_time_get());
            fflush(stdout);
            os_time_delay(5 * 1000);

            /* Release the semaphore */
            err = os_sem_release(&g_sem1);
            assert(err == OS_OK);
            printf("Task 10 releases semaphore 1 (ostime=%u)\n", os_time_get());
            fflush(stdout);
            sem_test_disp_sem(&g_sem1);

            /* Sleep to let other tasks run */
            printf("Task 10 sleeping for 5 seconds (ostime=%u)\n", 
                   os_time_get());
            fflush(stdout);
            os_time_delay(5 * 1000);
        }
    } else {
        while (1) {
            t = os_sched_get_current_task();
            assert(t->t_func == task10_handler);

            printf("Task 10 sleeping for 1000 secs (ostime=%u)\n", 
                   os_time_get());
            fflush(stdout);
            os_time_delay(1000 * 1000);
        }
    }
}

void 
task11_handler(void *arg) 
{
    os_error_t err;
    struct os_task *t;

    if (g_sem_test == 10) {
        while (1) {
            t = os_sched_get_current_task();
            assert(t->t_func == task11_handler);

            printf("Task 11 Loop (ostime=%u)\n", os_time_get());
            err = os_sem_pend(&g_sem1, 10000);
            assert(err == OS_OK);
            printf("Task 11 gets semaphore 1 (ostime=%u)\n", os_time_get());
            fflush(stdout);
            sem_test_disp_sem(&g_sem1);

            printf("Task 11 releases semaphore 1\n");
            fflush(stdout);
            err = os_sem_release(&g_sem1);
            assert(err == OS_OK);

            printf("Task 11 sleeping for 5 secs (ostime=%u)\n", os_time_get());
            fflush(stdout);
            os_time_delay(1000 * 5);
        }
    } else {
        if (g_sem_test == 12) {
            printf("Task 11 sleeps for 5 seconds (ostime=%u)\n", os_time_get());
            fflush(stdout);

            /* Sleep for 3 seconds */
            t = os_sched_get_current_task();;
            os_time_delay(5 * 1000);
        } else if (g_sem_test == 13) {
            printf("Task 11 sleeps for 3 seconds (ostime=%u)\n", os_time_get());
            fflush(stdout);

            /* Sleep for 3 seconds */
            t = os_sched_get_current_task();;
            os_time_delay(3 * 1000);
        }

        while (1) {
            t = os_sched_get_current_task();
            assert(t->t_func == task11_handler);

            printf("Task 11 wakes (ostime=%u)\n", os_time_get());
            fflush(stdout);

            err = os_sem_pend(&g_sem1, 1000*1000);
            if (g_sem_test == 14) {
                assert(err == OS_TIMEOUT);
            } else {
                assert(err == OS_OK);
            }

            if (err == OS_OK) {
                printf("Task 11 gets semaphore (ostime=%u)\n", os_time_get());
                fflush(stdout);
                err = os_sem_release(&g_sem1);
                assert(err == OS_OK);
            } else {
                printf("Task 11 fails to get semaphore (ostime=%u)\n",
                       os_time_get());
                fflush(stdout);
            }

            printf("Task 11 sleeping for 1000 secs (ostime=%u)\n",os_time_get());
            fflush(stdout);
            os_time_delay(1000 * 1000);
        }
    }
}

void 
task12_handler(void *arg) 
{
    os_error_t err;
    struct os_task *t;

    if (g_sem_test == 10) {
        while (1) {
            t = os_sched_get_current_task();
            assert(t->t_func == task12_handler);

            printf("Task 12 Loop (ostime=%u)\n", os_time_get());
            fflush(stdout);

            /* Get semaphore 1 */
            err = os_sem_pend(&g_sem1, OS_TIMEOUT_NEVER);
            assert(err == OS_OK);
            printf("Task 12 got semaphore (ostime-%u)\n", os_time_get());
            fflush(stdout);

            printf("Task 12 release semaphore (ostime=%u)\n", os_time_get());
            fflush(stdout);

            err = os_sem_release(&g_sem1);
            assert(err == OS_OK);

            printf("Task 12 sleeping for 5 secs (ostime=%u)\n", os_time_get());
            fflush(stdout);
            os_time_delay(1000 * 5);
        }
    } else {
        if (g_sem_test == 12) {
            printf("Task 12 sleeps for 3 seconds (ostime=%u)\n", os_time_get());
            fflush(stdout);

            /* Sleep for 3 seconds */
            t = os_sched_get_current_task();;
            os_time_delay(3 * 1000);
        } else if (g_sem_test == 13) {
            printf("Task 12 sleeps for 5 seconds (ostime=%u)\n", os_time_get());
            fflush(stdout);

            /* Sleep for 3 seconds */
            t = os_sched_get_current_task();;
            os_time_delay(5 * 1000);
        }

        while (1) {
            t = os_sched_get_current_task();
            assert(t->t_func == task12_handler);

            printf("Task 12 wakes (ostime=%u)\n", os_time_get());
            fflush(stdout);

            err = os_sem_pend(&g_sem1, 1000*1000);
            if (g_sem_test == 14) {
                assert(err == OS_TIMEOUT);
            } else {
                assert(err == OS_OK);
            }

            if (err == OS_OK) {
                printf("Task 12 gets semaphore (ostime=%u)\n", os_time_get());
                fflush(stdout);
                err = os_sem_release(&g_sem1);
                assert(err == OS_OK);
            } else {
                printf("Task 12 fails to get semaphore (ostime=%u)\n",
                       os_time_get());
                fflush(stdout);
            }

            printf("Task 12 sleeping for 1000 secs (ostime=%u)\n",
                   os_time_get());
            fflush(stdout);
            os_time_delay(1000 * 1000);
        }
    }
}

void 
task13_handler(void *arg) 
{
    os_error_t err;
    struct os_task *t;

    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task13_handler);

        printf("Task 13 wakes (ostime=%u)\n", os_time_get());
        fflush(stdout);
        if (g_sem_test == 15) {
            err = os_sem_pend(&g_sem1, 1*1000);
        } else {
            err = os_sem_pend(&g_sem1, 1000*1000);
        }

        if ((g_sem_test == 14) || (g_sem_test == 15)) {
            assert(err == OS_TIMEOUT);
        } else {
            assert(err == OS_OK);
        }

        if (err == OS_OK) {
            printf("Task 13 gets semaphore (ostime=%u)\n", os_time_get());
            fflush(stdout);
            err = os_sem_release(&g_sem1);
            assert(err == OS_OK);
        } else {
            printf("Task 13 fails to get semaphore (ostime=%u)\n", 
                   os_time_get());
            fflush(stdout);
        }

        printf("Task 13 sleeping for 1000 secs (ostime=%u)\n", os_time_get());
        fflush(stdout);
        os_time_delay(1000 * 1000);
    }
}

void
os_sem_test(int test_num)
{
    os_error_t err;

    g_sem_test = test_num;
    g_task10_val = 0;
    g_task11_val = 0;
    g_task12_val = 0;

    if ((test_num < 10) || (test_num >= 20)) {
        printf("Invalid test number %u! Should be between 10 and 20\n",
               test_num);
        fflush(stdout);
        exit(0);
    }

    err = os_sem_create(&g_sem1, 1);
    assert(err == OS_OK);
    err = os_sem_create(&g_sem2, 1);
    assert(err == OS_OK);

    os_task_init(&task10, "task10", task10_handler, NULL, TASK10_PRIO, stack10, 
                 OS_STACK_ALIGN(1024));

    os_task_init(&task11, "task11", task11_handler, NULL, TASK11_PRIO, stack11, 
                 OS_STACK_ALIGN(1024));

    os_task_init(&task12, "task12", task12_handler, NULL, TASK12_PRIO, stack12, 
                 OS_STACK_ALIGN(1024));

    if (test_num != 10) {
        os_task_init(&task13, "task13", task13_handler, NULL, TASK13_PRIO, 
                     stack13, OS_STACK_ALIGN(1024));
    }
}

