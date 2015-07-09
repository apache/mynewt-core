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

#include <string.h>

void
os_eventq_init(struct os_eventq *evq)
{
    memset(evq, 0, sizeof(*evq));
    TAILQ_INIT(&evq->evq_list);
}

void
os_eventq_put2(struct os_eventq *evq, struct os_event *ev, int isr)
{
    os_sr_t sr; 

    if (!isr) {
        OS_ENTER_CRITICAL(sr);
    }

    /* Do not queue if already queued */
    if (ev->ev_queued) {
        if (!isr) {
            OS_EXIT_CRITICAL(sr);
        }
        return;
    }
    ev->ev_queued = 1; 

    TAILQ_INSERT_TAIL(&evq->evq_list, ev, ev_next);
    if (evq->evq_task) {
        if (!isr) {
            OS_EXIT_CRITICAL(sr);
        }
        os_sched_wakeup(evq->evq_task, 1, isr);
    } else {
        if (!isr) {
            OS_EXIT_CRITICAL(sr);
        }
    }
}

void 
os_eventq_put(struct os_eventq *evq, struct os_event *ev) 
{
    os_eventq_put2(evq, ev, 0);
}

struct os_event * 
os_eventq_get(struct os_eventq *evq)
{
    struct os_event *ev; 
    os_sr_t sr; 

    OS_ENTER_CRITICAL(sr);
pull_one:
    ev = TAILQ_FIRST(&evq->evq_list);
    if (ev) {
        TAILQ_REMOVE(&evq->evq_list, ev, ev_next);
        ev->ev_queued = 0;
    } else {
        evq->evq_task = os_sched_get_current_task();
        /* put this task to sleep for a long time... 
         * XXX: implement sleep until woken, right now requires a timeout 
         */
        OS_EXIT_CRITICAL(sr); 
        os_sched_sleep(evq->evq_task, OS_TIMEOUT_NEVER);
        /* Sched sleep returns when the task has been woken up */
        OS_ENTER_CRITICAL(sr); 
        evq->evq_task = NULL;
        goto pull_one;
    }
    OS_EXIT_CRITICAL(sr); 

    return (ev);
}

void
os_eventq_remove(struct os_eventq *evq, struct os_event *ev)
{
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    TAILQ_REMOVE(&evq->evq_list, ev, ev_next); 
    OS_EXIT_CRITICAL(sr);
    ev->ev_queued = 0;
}
