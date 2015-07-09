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

        os_sched_sleep(t, 1000);
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
 * Main for the given project. Main initializes the OS, calls init tasks, then 
 * calls os start to start the tasks running. 
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

    /* We should never get here! */
    assert(0);

    return rc;
}

