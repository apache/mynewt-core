/******************************************************************************
** Copyright Silver Spring Networks 2004 - 2010
** All rights reserved.
**
** $Id: main.c 76842 2015-03-26 05:00:46Z wills $
******************************************************************************/
#include "os/os.h"
#include "../../../hal/include/hal/hal_gpio.h"
#include <assert.h>

/* Init all tasks */
volatile int tasks_initialized;
int init_tasks(void);

static volatile int g_task1_loops;

#define TASK1_PRIO (1) 
struct os_task task1;
os_stack_t stack1[OS_STACK_ALIGN(1024)];

int led_pin;

void 
task1_handler(void *arg)
{
    int led_state;
    struct os_task *t;

    /* Set the led pin for the E407 devboard */
    led_pin = 45;
    gpio_init_out(led_pin, 1);

    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task1_handler);

        ++g_task1_loops;

        os_time_delay(1000);
        gpio_toggle(led_pin);
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
    os_task_init(&task1, "task1", task1_handler, NULL, 
            TASK1_PRIO, stack1, OS_STACK_ALIGN(1024));
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

