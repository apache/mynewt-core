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
#include "os/os.h"
#include "hal/hal_gpio.h"
#include "hal/hal_cputime.h"
#include <assert.h>
#include <string.h>


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
static volatile int g_task2_loops;

/* Task 3 */
#define TASK3_PRIO (3) 
#define TASK3_STACK_SIZE    OS_STACK_ALIGN(1024)
struct os_task task3;
os_stack_t stack3[TASK3_STACK_SIZE];
static volatile int g_task3_loops;
struct timer_test_data
{
    int32_t dt;
    uint32_t cntr;
    uint32_t cputime;
};

struct timer_test_data g_tcpu1_data;
struct timer_test_data g_tcpu2_data;
struct timer_test_data g_tcpu3_data;
uint32_t g_timeout;
int32_t g_dt;
void test_timer_cb(void *arg);
void tcpu4_cb(void *arg);

#define OS_EVENT_T_TIMER_TEST (OS_EVENT_T_PERUSER) 
struct os_event g_timer_test_ev = 
    OS_EVENT_INITIALIZER(timer_test, OS_EVENT_T_TIMER_TEST, NULL);
struct os_eventq g_timer_test_evq;
uint32_t g_timer_events;

/* Global test semaphore */
struct os_sem g_test_sem;

/* For LED toggling */
int g_led_pin;

/* For GPIO test code */
int g_gpio_test_out_pin;
int g_gpio_test_in_pin;
uint32_t g_gpio_test_irqs;

/* For cputimer test code */
uint32_t g_late_ticks;

void
gpio_test_irq(void *arg)
{
    if (arg == NULL) {
        assert(0);
    }
    ++g_gpio_test_irqs;

    if (g_gpio_test_irqs > 1) {
        os_sem_release(&g_test_sem);
    }
}

static void
cputime_test(void)
{
    os_sr_t sr;
    uint32_t temp;
    uint32_t ticks;

    temp = cputime_nsecs_to_ticks(50);
    assert(temp == 5);
    temp = cputime_ticks_to_nsecs(4);
    assert(temp == 48);

    temp = cputime_usecs_to_ticks(1);
    assert(temp == 84);
    temp = cputime_ticks_to_usecs(1000);
    assert(temp == 12);

    /* Test delays */
    OS_ENTER_CRITICAL(sr);
    temp = cputime_get32();
    cputime_delay_usecs(30);
    ticks = cputime_get32() - temp;
    temp = cputime_ticks_to_usecs(ticks);
    g_late_ticks = temp;
    assert((uint32_t)(temp - 30) <= 1UL);

    temp = cputime_get32();
    cputime_delay_ticks(5000);
    ticks = cputime_get32() - temp;
    assert(ticks > 5000);
    g_late_ticks = ticks - 5000;
    OS_EXIT_CRITICAL(sr);
}

void
gpio_test(void)
{
    int rc;

    /* Set gpio test pins */
    g_gpio_test_out_pin = 34;
    gpio_init_out(g_gpio_test_out_pin, 0);

    /* Set up the gpio test input pin */
    g_gpio_test_in_pin = 26;
    rc = gpio_irq_init(g_gpio_test_in_pin, gpio_test_irq, &g_gpio_test_irqs,
                       GPIO_TRIG_RISING, GPIO_PULL_DOWN);
    assert(rc == 0);

    /* Make sure no interrupts yet */
    assert(g_gpio_test_irqs == 0);

    /* Read test input pin. Should read low */
    rc = gpio_read(g_gpio_test_in_pin);
    assert(rc == 0);

    /* Set output high */
    gpio_set(g_gpio_test_out_pin);

    /* Make sure only one interrupt occurred */
    assert(g_gpio_test_irqs == 1);

    /* Set output low */
    gpio_clear(g_gpio_test_out_pin);

    /* Make sure no interrupt occurred */
    assert(g_gpio_test_irqs == 1);
}

void 
task1_handler(void *arg)
{
    struct os_task *t;

    /* Set the led pin for the E407 devboard */
    g_led_pin = 45;
    gpio_init_out(g_led_pin, 1);

    /* Test gpio */
    gpio_test();

    /* Perform some cpu time tests */
    cputime_test();

    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task1_handler);

        ++g_task1_loops;

        os_time_delay(1000);

        /* Toggle the LED */
        gpio_toggle(g_led_pin);

        /* Toggle the test gpio to create an interrupt (every other toggle) */
        gpio_toggle(g_gpio_test_out_pin);
    }
}

void 
task2_handler(void *arg)
{
    struct os_task *t;

    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task2_handler);

        /* Wait for semaphore from ISR */
        os_sem_pend(&g_test_sem, OS_TIMEOUT_NEVER);

        /* Increment # of times we went through task loop */
        ++g_task2_loops;
    }
}

void
test_timer_cb(void *arg)
{
    struct timer_test_data *tdata;

    tdata = (struct timer_test_data *)arg;
    tdata->cputime = cputime_get32();
    tdata->cntr += 1;
}

void
tcpu4_cb(void *arg)
{
    os_eventq_put(&g_timer_test_evq, (struct os_event *)arg);
}

void 
task3_handler(void *arg)
{
    int tnum;
    int32_t dt;
    uint32_t timeout;
    uint32_t ostime;
    struct os_event *ev;
    struct os_task *t;
    struct cpu_timer tcpu1, tcpu2, tcpu3, tcpu4;

    /* Initialize eventq */
    os_eventq_init(&g_timer_test_evq);

    /* We dont zero out ccm yet... */
    memset(&g_tcpu1_data, 0, sizeof(struct timer_test_data));
    memset(&g_tcpu2_data, 0, sizeof(struct timer_test_data));
    memset(&g_tcpu3_data, 0, sizeof(struct timer_test_data));

    tnum = 0;
    cputime_timer_init(&tcpu1, test_timer_cb, &g_tcpu1_data);
    cputime_timer_init(&tcpu2, test_timer_cb, &g_tcpu2_data);
    cputime_timer_init(&tcpu3, test_timer_cb, &g_tcpu3_data);
    cputime_timer_init(&tcpu4, tcpu4_cb, &g_timer_test_ev);

    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task3_handler);

        /* Increment # of times we went through task loop */
        ++g_task3_loops;

        /* Start a timer */
        switch (tnum) {
        case 0:
            /* Start a timer that will expire in the future */
            timeout = cputime_get32() + cputime_usecs_to_ticks(330);
            cputime_timer_start(&tcpu1, timeout);

            /* Wait to get timeout */
            cputime_delay_usecs(350);
            dt = (int32_t)(cputime_get32() - timeout);
            assert(dt >= 0);
            
            /* Better have count incremented! */
            assert(g_tcpu1_data.cntr == 1);

            tnum = 1;
            break;
        case 1:
            /* Start three timers and make sure they fire off in correct
               order */
            timeout = cputime_get32();
            cputime_timer_start(&tcpu1, timeout + cputime_usecs_to_ticks(330));
            cputime_timer_start(&tcpu2, timeout + cputime_usecs_to_ticks(980));
            cputime_timer_start(&tcpu3, timeout + cputime_usecs_to_ticks(733));
            g_timeout = timeout;

            /* Test that we set cputime correctly */
            if (tcpu1.cputime != timeout + cputime_usecs_to_ticks(330)) {
                assert(0);
            }

            /* Test that we set cputime correctly */
            if (tcpu2.cputime != timeout + cputime_usecs_to_ticks(980)) {
                assert(0);
            }

            /* Test that we set cputime correctly */
            if (tcpu3.cputime != timeout + cputime_usecs_to_ticks(733)) {
                assert(0);
            }

            /* Better not have incremented! */
            assert(g_tcpu1_data.cntr == 1);
            assert(g_tcpu2_data.cntr == 0);
            assert(g_tcpu3_data.cntr == 0);

            /* Wait to get timeout */
            cputime_delay_usecs(1000);
            dt = (int32_t)(cputime_get32() - timeout);
            assert(dt >= 0);

            /* Better have count incremented! */
            assert(g_tcpu1_data.cntr == 2);
            assert(g_tcpu2_data.cntr == 1);
            assert(g_tcpu3_data.cntr == 1);

            /* Check correct order */
            if (CPUTIME_LEQ(g_tcpu2_data.cputime, g_tcpu3_data.cputime)) {
                assert(0);
            }
            if (CPUTIME_LEQ(g_tcpu2_data.cputime, g_tcpu1_data.cputime)) {
                assert(0);
            }
            if (CPUTIME_LEQ(g_tcpu3_data.cputime, g_tcpu1_data.cputime)) {
                assert(0);
            }

            /* Make sure that we woke up close to when we expected */
            if (CPUTIME_LT(g_tcpu1_data.cputime, tcpu1.cputime)) {
                assert(0);
            }
            dt = g_tcpu1_data.cputime - tcpu1.cputime;
            if (dt > cputime_usecs_to_ticks(1)) {
                assert(0);
            }
            g_tcpu1_data.dt = dt;

            if (CPUTIME_LT(g_tcpu2_data.cputime, tcpu2.cputime)) {
                assert(0);
            }
            dt = g_tcpu2_data.cputime - tcpu2.cputime;
            if (dt > cputime_usecs_to_ticks(1)) {
                assert(0);
            }
            g_tcpu2_data.dt = dt;

            if (CPUTIME_LT(g_tcpu3_data.cputime, tcpu3.cputime)) {
                assert(0);
            }
            dt = g_tcpu3_data.cputime - tcpu3.cputime;
            if (dt > cputime_usecs_to_ticks(1)) {
                assert(0);
            }
            g_tcpu3_data.dt = dt;

            tnum = 2;
            break;
        case 2:
            /* Make sure counters are what we expect */
            assert(g_tcpu1_data.cntr == 2);
            assert(g_tcpu2_data.cntr == 1);
            assert(g_tcpu3_data.cntr == 1);

            ostime = os_time_get();
            timeout = cputime_get32() + cputime_usecs_to_ticks(3700000);
            cputime_timer_start(&tcpu4, timeout);

            ev = os_eventq_get(&g_timer_test_evq);
            switch (ev->ev_type) {
            case OS_EVENT_T_TIMER_TEST:
                dt = (int32_t)(os_time_get() - ostime); 
                if (dt < 0) {
                    g_dt = dt;
                    assert(0);
                }
                if ((dt != 3700) && (dt != 3701)) {
                    g_dt = dt;
                    assert(0);
                }
                ++g_timer_events;
                break;
            default:
                assert(0);
            }

            if (g_timer_events > 4) {
                tnum = 3;
            }
            break;
        case 3:
            timeout = cputime_get32();
            cputime_timer_start(&tcpu1, timeout + cputime_usecs_to_ticks(666));
            cputime_timer_start(&tcpu2, timeout + cputime_usecs_to_ticks(555));
            cputime_timer_start(&tcpu3, timeout + cputime_usecs_to_ticks(444));

            /* Make sure counters are what we expect */
            assert(g_tcpu1_data.cntr == 2);
            assert(g_tcpu2_data.cntr == 1);
            assert(g_tcpu3_data.cntr == 1);

            /* Remove tcpu1 */
            cputime_delay_usecs(200);
            cputime_timer_stop(&tcpu1);

            /* Make sure counters are what we expect */
            assert(g_tcpu1_data.cntr == 2);
            assert(g_tcpu2_data.cntr == 1);
            assert(g_tcpu3_data.cntr == 1);
            cputime_delay_nsecs(1000000);

            /* Make sure appropriate counters got set */
            assert(g_tcpu1_data.cntr == 2);
            assert(g_tcpu2_data.cntr == 2);
            assert(g_tcpu3_data.cntr == 2);

            if (CPUTIME_LEQ(g_tcpu2_data.cputime, g_tcpu3_data.cputime)) {
                assert(0);
            }
            tnum = 4;
            break;
        case 4:
            /* Make sure appropriate counters got set */
            assert(g_tcpu1_data.cntr == 2);
            assert(g_tcpu2_data.cntr == 2);
            assert(g_tcpu3_data.cntr == 2);
            os_time_delay(3333);
            break;
        default:
            assert(0);
            break;
        }
    }
}

/**
 * init_tasks
 *  
 * This function is called by the sanity task to initialize all system tasks. 
 * The code locks the OS scheduler to prevent context switch and then calls 
 * all the task init routines. 
 *  
 * @return int 0 success; error otherwise.
 */
int
init_tasks(void)
{
    /* Create global test semaphore */
    os_sem_create(&g_test_sem, 0);

    os_task_init(&task1, "task1", task1_handler, NULL, 
            TASK1_PRIO, stack1, TASK1_STACK_SIZE);

    os_task_init(&task2, "task2", task2_handler, NULL, 
            TASK2_PRIO, stack2, TASK2_STACK_SIZE);

    os_task_init(&task3, "task3", task3_handler, NULL, 
            TASK3_PRIO, stack3, TASK3_STACK_SIZE);

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
main(void)
{
    int rc;

    /* Initialize cpu time */
    rc = cputime_init(84000000);
    assert(rc == 0);

    os_init();
    rc = init_tasks();
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return rc;
}

