/******************************************************************************
** Copyright Silver Spring Networks 2004 - 2010
** All rights reserved.
**
** $Id: main.c 76842 2015-03-26 05:00:46Z wills $
******************************************************************************/
#include "os/os.h"
#include <assert.h>

/* Init all tasks */
volatile int tasks_initialized;
int init_tasks(void);

static volatile int g_task1_loops;

#define TASK1_PRIO (1) 
struct os_task task1;
os_stack_t stack1[OS_STACK_ALIGN(1024)]; 

void 
task1_handler(void *arg)
{
    int led_state;
    struct os_task *t; 

    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task1_handler);

        ++g_task1_loops;

        os_time_delay(1000);
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
 * Main for the given project. Main initializes the OS, the system timer and 
 * the "sanity" task. The sanity task is a software watchdog, making sure all 
 * running tasks are either checking in or waiting on an event. The sanity task 
 * also initializes all of the UCOS tasks, so that initialization is done at the
 * priority task level. 
 *  
 * @return int NOTE: this function should never return!
 */
volatile int main_loop_counter;

int
main(void)
{
    int rc;

    os_init();
    rc = init_tasks();
    os_start();

    /* Dont think we should ever get here */
    while (1) {
        ++main_loop_counter;
    }
    return rc;
}

