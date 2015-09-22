/**
 * Copyright (c) 2015 Runtime Inc.
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

static os_time_t g_os_time = 0;

os_time_t  
os_time_get(void)
{
    return (g_os_time);
}

/**
 * Called for every single tick by the architecture specific functions.
 *
 * Increases the os_time by 1 tick. 
 */
void
os_time_tick(void)
{
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    ++g_os_time;
    OS_EXIT_CRITICAL(sr);
}

/**
 * Puts the current task to sleep for the specified number of os ticks. There 
 * is no delay if ticks is <= 0. 
 * 
 * @param osticks Number of ticks to delay (<= 0 means no delay).
 */
void
os_time_delay(int32_t osticks)
{
    os_sr_t sr;

    if (osticks > 0) {
        OS_ENTER_CRITICAL(sr);
        os_sched_sleep(os_sched_get_current_task(), (os_time_t)osticks);
        OS_EXIT_CRITICAL(sr);
        os_sched(NULL, 0);
    }
}
