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
#include "os/queue.h"

#include <assert.h> 

struct os_task g_idle_task; 
os_stack_t g_idle_task_stack[OS_IDLE_STACK_SIZE]; 

uint32_t g_os_idle_ctr;

void
os_idle_task(void *arg)
{
    os_sr_t sr; 

    while (1) {
        /* XXX: do we really need to enable/disable interrupts here? */ 
        OS_ENTER_CRITICAL(sr);
        ++g_os_idle_ctr;
        OS_EXIT_CRITICAL(sr);
    }
}

void
os_init_idle_task(void)
{
    os_task_init(&g_idle_task, "idle", os_idle_task, NULL, 
            OS_IDLE_PRIO, g_idle_task_stack, OS_IDLE_STACK_SIZE);
}

void
os_init(void)
{
    os_error_t err;

    err = os_arch_os_init();
    assert(err == OS_OK);
}

void
os_start(void)
{
    os_error_t err;

    err = os_arch_os_start();
    assert(err == OS_OK);

    while (1) {
        /* XXX: should probably assert or exit() with an error here. */
    }
}
