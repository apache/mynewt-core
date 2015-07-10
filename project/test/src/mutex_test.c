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
#include "os/os_mutex.h"

struct os_task task4;
os_stack_t stack4[OS_STACK_ALIGN(1028)]; 

struct os_task task5;
os_stack_t stack5[OS_STACK_ALIGN(1028)];

struct os_task task6;
os_stack_t stack6[OS_STACK_ALIGN(1028)];

struct os_task task7;
os_stack_t stack7[OS_STACK_ALIGN(1028)];

#define TASK4_PRIO (4) 
#define TASK5_PRIO (5) 
#define TASK6_PRIO (6) 
#define TASK7_PRIO (7) 

volatile int g_task4_val;
volatile int g_task5_val;
volatile int g_task6_val;
volatile int g_task7_val;
volatile int g_task5_print;
struct os_mutex g_mutex1;
struct os_mutex g_mutex2;

static volatile int g_mutex_test;

/**
 * mutex test disp mutex
 *  
 * Display mutex contents 
 * 
 * @param mu 
 */
void
mutex_test_disp_mutex(struct os_mutex *mu)
{
    printf ("\tMutex: owner=%p level=%u prio=%u, head=%p\n",
           mu->mu_owner, mu->mu_level, mu->mu_prio, SLIST_FIRST(&mu->mu_head));
    fflush(stdout);
}

/**
 * mutex test basic 
 *  
 * Basic mutex tests
 * 
 * @return int 
 */
static int 
mutex_test_basic(struct os_mutex *mu, struct os_task *t)
{
    os_error_t err;

    /* Test errors first */
    printf("Performing basic mutex testing\n");

    /* Test some error cases */
    err = os_mutex_create(NULL);
    if (err != OS_INVALID_PARM) {
        printf("Error: expected error from mutex create (err=%d)\n", err);
        goto mutex_err_exit;
    }
    err = os_mutex_delete(NULL);
    if (err != OS_INVALID_PARM) {
        printf("Error: expected error from mutex delete (err=%d)\n", err);
        goto mutex_err_exit;
    }
    err = os_mutex_release(NULL);
    if (err != OS_INVALID_PARM) {
        printf("Error: expected error from mutex release (err=%d)\n", err);
        goto mutex_err_exit;
    }
    err = os_mutex_pend(NULL, 0);
    if (err != OS_INVALID_PARM) {
        printf("Error: expected error from mutex pend (err=%d)\n", err);
        goto mutex_err_exit;
    }

    /* Get the mutex */
    err = os_mutex_pend(mu, 0);
    if (err) {
        printf("Error: did not get free mutex immediately (err=%d)\n", err);
    }

    /* Check mutex internals */
    if ((mu->mu_owner != t) || (mu->mu_level != 1) || 
            (mu->mu_prio != t->t_prio) || !SLIST_EMPTY(&mu->mu_head)) {
        printf("Error: mutex internals not correct after getting mutex\n");
        printf("Mutex: owner=%p prio=%u level=%u head=%p\n",
               mu->mu_owner, mu->mu_prio, mu->mu_level, 
               SLIST_FIRST(&mu->mu_head));
        printf("Task: task=%p prio=%u\n", t, t->t_prio);
        goto mutex_err_exit;
    }

    /* Get the mutex again; should be level 2 */
    err = os_mutex_pend(mu, 0);
    if (err) {
        printf("Error: did not get my mutex immediately (err=%d)\n", err);
    }

    /* Check mutex internals */
    if ((mu->mu_owner != t) || (mu->mu_level != 2) || 
            (mu->mu_prio != t->t_prio) || !SLIST_EMPTY(&mu->mu_head)) {
        printf("Error: mutex internals not correct after getting mutex\n");
        printf("Mutex: owner=%p prio=%u level=%u head=%p\n",
               mu->mu_owner, mu->mu_prio, mu->mu_level, 
               SLIST_FIRST(&mu->mu_head));
        printf("Task: task=%p prio=%u\n", t, t->t_prio);
        goto mutex_err_exit;
    }

    /* Release mutex */
    err = os_mutex_release(mu);
    if (err) {
        printf("Error: could not release mutex I own (err=%d)\n", err);
        goto mutex_err_exit;
    }
    /* Check mutex internals */
    if ((mu->mu_owner != t) || (mu->mu_level != 1) || 
            (mu->mu_prio != t->t_prio) || !SLIST_EMPTY(&mu->mu_head)) {
        printf("Error: mutex internals not correct after getting mutex\n");
        printf("Mutex: owner=%p prio=%u level=%u head=%p\n",
               mu->mu_owner, mu->mu_prio, mu->mu_level, 
               SLIST_FIRST(&mu->mu_head));
        printf("Task: task=%p prio=%u\n", t, t->t_prio);
        goto mutex_err_exit;
    }

    /* Release it again */
    err = os_mutex_release(mu);
    if (err) {
        printf("Error: could not release mutex I own (err=%d)\n", err);
        goto mutex_err_exit;
    }
    /* Check mutex internals */
    if ((mu->mu_owner != NULL) || (mu->mu_level != 0) || 
            (mu->mu_prio != t->t_prio) || !SLIST_EMPTY(&mu->mu_head)) {
        printf("Error: mutex internals not correct after getting mutex\n");
        printf("Mutex: owner=%p prio=%u level=%u head=%p\n",
               mu->mu_owner, mu->mu_prio, mu->mu_level, 
               SLIST_FIRST(&mu->mu_head));
        printf("Task: task=%p prio=%u\n", t, t->t_prio);
        goto mutex_err_exit;
    }

    printf("Finished basic mutex testing.\n");

    return 0;

mutex_err_exit:
    return -1;
}

void 
task4_handler(void *arg)
{
    os_error_t err;
    struct os_task *t;

    /* Do some basic mutex testing first */
    if (mutex_test_basic(&g_mutex1, os_sched_get_current_task())) {
        exit(0);
    }

    if (g_mutex_test == 1) {
        while (1) {
            t = os_sched_get_current_task();
            assert(t->t_func == task4_handler);

            printf("Task 4 sleeping for 10 secs (ostime=%u)\n", os_time_get());
            fflush(stdout);
            os_time_delay(1000 * 10);

            printf("Task 4 wakes (ostime=%u)\n", os_time_get());
            g_task4_val = 1;
            err = os_mutex_pend(&g_mutex1, 10 * 1000);
            assert(err == OS_OK);
            assert(g_task6_val == 1);
            printf("Task 4 gets mutex 1 (ostime=%u)\n", os_time_get());
            fflush(stdout);
            mutex_test_disp_mutex(&g_mutex1);

            printf("Task 4 sleeping for 1000 seconds (ostime=%u)\n", 
                   os_time_get());
            fflush(stdout);

            /* Allow task5 to display a message */
            g_task5_print = 1;
            os_time_delay(1000 * 1000);
        }
    } else {
        while (1) {
            t = os_sched_get_current_task();
            assert(t->t_func == task4_handler);

            printf("Task 4 wakes (ostime=%u)\n", os_time_get());
            err = os_mutex_pend(&g_mutex1, 0);
            assert(err == OS_OK);

            printf("Task 4 sleeping for 10 secs (ostime=%u)\n", os_time_get());
            fflush(stdout);
            g_task4_val = 1;
            os_time_delay(1000 * 10);

            if (g_mutex_test == 4) {
                printf("Task 4 wakes (ostime=%u)\n", os_time_get());
                mutex_test_disp_mutex(&g_mutex1);

                printf("Task 4 deleting mutex %p (ostime=%u)\n", 
                       &g_mutex1, os_time_get());
                fflush(stdout);
                os_mutex_delete(&g_mutex1);
                printf("Task 4 sleeping for 1000 secs (ostime=%u)\n", os_time_get());
                fflush(stdout);
                os_time_delay(1000 * 1000);
            }
            printf("Task 4 wakes (ostime=%u)\n", os_time_get());
            mutex_test_disp_mutex(&g_mutex1);

            os_mutex_release(&g_mutex1);
            printf("Task 4 sleeping for 1000 secs (ostime=%u)\n", os_time_get());
            fflush(stdout);
            os_time_delay(1000 * 1000);
        }
    }
}

void 
task5_handler(void *arg) 
{
    os_error_t err;
    struct os_task *t;

    if (g_mutex_test == 1) {
        while (1) {
            t = os_sched_get_current_task();
            assert(t->t_func == task5_handler);

            printf("Task 5 sleeping for 5 secs (ostime=%u)\n", os_time_get());
            fflush(stdout);
            os_time_delay(1000 * 5);
            printf("Task 5 wakes (ostime=%u)\n", os_time_get());
            fflush(stdout);
            g_task5_print = 0;
            while (1) {
                /* Wait here forever */
                if (g_task5_print) {
                    printf("Task5 loop\n");
                    fflush(stdout);
                    g_task5_print = 0;
                }
            }
        }
    } else {
        if (g_mutex_test == 2) {
            printf("Task 5 sleeps for 5 seconds (ostime=%u)\n", os_time_get());
            fflush(stdout);

            /* Sleep for 3 seconds */
            t = os_sched_get_current_task();;
            os_time_delay(5 * 1000);
        } else if (g_mutex_test == 3) {
            printf("Task 5 sleeps for 3 seconds (ostime=%u)\n", os_time_get());
            fflush(stdout);

            /* Sleep for 3 seconds */
            t = os_sched_get_current_task();;
            os_time_delay(3 * 1000);
        }

        while (1) {
            t = os_sched_get_current_task();
            assert(t->t_func == task5_handler);

            printf("Task 5 wakes (ostime=%u)\n", os_time_get());
            fflush(stdout);

            err = os_mutex_pend(&g_mutex1, 1000*1000);
            if (g_mutex_test == 4) {
                assert(err == OS_TIMEOUT);
            } else {
                assert(err == OS_OK);
            }

            if (err == OS_OK) {
                printf("Task 5 gets mutex (ostime=%u)\n", os_time_get());
                fflush(stdout);
                err = os_mutex_release(&g_mutex1);
                assert(err == OS_OK);
            } else {
                printf("Task 5 fails to get mutex (ostime=%u)\n",os_time_get());
                fflush(stdout);
            }

            printf("Task 5 sleeping for 1000 secs (ostime=%u)\n",os_time_get());
            fflush(stdout);
            os_time_delay(1000 * 1000);
        }
    }
}

void 
task6_handler(void *arg) 
{
    os_error_t err;
    struct os_task *t;

    if (g_mutex_test == 1) {
        while (1) {
            t = os_sched_get_current_task();
            assert(t->t_func == task6_handler);

            printf("Task 6 Loop (ostime=%u)\n", os_time_get());
            fflush(stdout);

            /* Get mutex 1 */
            err = os_mutex_pend(&g_mutex1, 0xFFFFFFFF);
            assert(err == OS_OK);
            printf("Task 6 got mutex (ostime-%u)\n", os_time_get());

            while (g_task4_val != 1) {
                /* Wait till task 1 wakes up and sets val. */
            }

            g_task6_val = 1;

            printf("Task 6 release mutex (ostime=%u)\n", os_time_get());
            fflush(stdout);

            err = os_mutex_release(&g_mutex1);
            assert(err == OS_OK);
        }
    } else {
        if (g_mutex_test == 2) {
            printf("Task 6 sleeps for 3 seconds (ostime=%u)\n", os_time_get());
            fflush(stdout);

            /* Sleep for 3 seconds */
            t = os_sched_get_current_task();;
            os_time_delay(3 * 1000);
        } else if (g_mutex_test == 3) {
            printf("Task 6 sleeps for 5 seconds (ostime=%u)\n", os_time_get());
            fflush(stdout);

            /* Sleep for 3 seconds */
            t = os_sched_get_current_task();;
            os_time_delay(5 * 1000);
        }

        while (1) {
            t = os_sched_get_current_task();
            assert(t->t_func == task6_handler);

            printf("Task 6 wakes (ostime=%u)\n", os_time_get());
            fflush(stdout);

            err = os_mutex_pend(&g_mutex1, 1000*1000);
            if (g_mutex_test == 4) {
                assert(err == OS_TIMEOUT);
            } else {
                assert(err == OS_OK);
            }

            if (err == OS_OK) {
                printf("Task 6 gets mutex (ostime=%u)\n", os_time_get());
                fflush(stdout);
                err = os_mutex_release(&g_mutex1);
                assert(err == OS_OK);
            } else {
                printf("Task 6 fails to get mutex (ostime=%u)\n",os_time_get());
                fflush(stdout);
            }

            printf("Task 6 sleeping for 1000 secs (ostime=%u)\n",os_time_get());
            fflush(stdout);
            os_time_delay(1000 * 1000);
        }
    }
}

void 
task7_handler(void *arg) 
{
    os_error_t err;
    struct os_task *t;

    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task7_handler);

        printf("Task 7 wakes (ostime=%u)\n", os_time_get());
        fflush(stdout);
        if (g_mutex_test == 5) {
            err = os_mutex_pend(&g_mutex1, 1*1000);
        } else {
            err = os_mutex_pend(&g_mutex1, 1000*1000);
        }

        if ((g_mutex_test == 4) || (g_mutex_test == 5)) {
            assert(err == OS_TIMEOUT);
        } else {
            assert(err == OS_OK);
        }

        if (err == OS_OK) {
            printf("Task 7 gets mutex (ostime=%u)\n", os_time_get());
            fflush(stdout);
            err = os_mutex_release(&g_mutex1);
            assert(err == OS_OK);
        } else {
            printf("Task 7 fails to get mutex (ostime=%u)\n", os_time_get());
            fflush(stdout);
        }

        printf("Task 7 sleeping for 1000 secs (ostime=%u)\n", os_time_get());
        fflush(stdout);
        os_time_delay(1000 * 1000);
    }
}

void
os_mutex_test(int test_num)
{
    g_mutex_test = test_num;
    g_task4_val = 0;
    g_task5_val = 0;
    g_task6_val = 0;
    os_mutex_create(&g_mutex1);
    os_mutex_create(&g_mutex2);

    os_task_init(&task4, "task4", task4_handler, NULL, TASK4_PRIO, stack4, 
                 OS_STACK_ALIGN(1024));

    os_task_init(&task5, "task5", task5_handler, NULL, TASK5_PRIO, stack5, 
                 OS_STACK_ALIGN(1024));

    os_task_init(&task6, "task6", task6_handler, NULL, TASK6_PRIO, stack6, 
                 OS_STACK_ALIGN(1024));

    if (test_num != 1) {
        os_task_init(&task7, "task7", task7_handler, NULL, TASK7_PRIO, stack7,
                     OS_STACK_ALIGN(1024));
    }
}


