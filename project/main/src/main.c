/******************************************************************************
** Copyright Silver Spring Networks 2004 - 2010
** All rights reserved.
**
** $Id: main.c 76842 2015-03-26 05:00:46Z wills $
******************************************************************************/
#include "os/os.h"
#include "hal/hal_gpio.h"
#include <assert.h>

/* Init all tasks */
volatile int tasks_initialized;
int init_tasks(void);

/* Task 1 */
#define TASK1_PRIO (1) 
struct os_task task1;
os_stack_t stack1[OS_STACK_ALIGN(1024)];
static volatile int g_task1_loops;

/* Task 2 */
#define TASK2_PRIO (2) 
struct os_task task2;
os_stack_t stack2[OS_STACK_ALIGN(1024)];
static volatile int g_task2_loops;

/* Global test semaphore */
struct os_sem g_test_sem;

/* For LED toggling */
int g_led_pin;

/* For GPIO test code */
int g_gpio_test_out_pin;
int g_gpio_test_in_pin;
uint32_t g_gpio_test_irqs;

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

void gpio_test(void)
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
            TASK1_PRIO, stack1, OS_STACK_ALIGN(1024));

    os_task_init(&task2, "task2", task2_handler, NULL, 
            TASK2_PRIO, stack2, OS_STACK_ALIGN(1024));
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

    os_init();
    rc = init_tasks();
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return rc;
}

