/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */


#include "os/os.h"

#include <string.h>

void
os_eventq_init(struct os_eventq *evq)
{
    memset(evq, 0, sizeof(*evq));
    STAILQ_INIT(&evq->evq_list);
}

void
os_eventq_put2(struct os_eventq *evq, struct os_event *ev, int isr)
{
    int resched;
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);

    /* Do not queue if already queued */
    if (OS_EVENT_QUEUED(ev)) {
        OS_EXIT_CRITICAL(sr);
        return;
    }

    /* Queue the event */
    ev->ev_queued = 1;
    STAILQ_INSERT_TAIL(&evq->evq_list, ev, ev_next);

    /* If task waiting on event, wake it up. */
    resched = 0;
    if (evq->evq_task) {
        os_sched_wakeup(evq->evq_task);
        evq->evq_task = NULL;
        resched = 1;
    }

    OS_EXIT_CRITICAL(sr);

    if (resched) {
        os_sched(NULL, isr);
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
    ev = STAILQ_FIRST(&evq->evq_list);
    if (ev) {
        STAILQ_REMOVE(&evq->evq_list, ev, os_event, ev_next);
        ev->ev_queued = 0;
    } else {
        evq->evq_task = os_sched_get_current_task();
        os_sched_sleep(evq->evq_task, OS_TIMEOUT_NEVER);
        OS_EXIT_CRITICAL(sr);

        os_sched(NULL, 0);

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
    if (OS_EVENT_QUEUED(ev)) {
        STAILQ_REMOVE(&evq->evq_list, ev, os_event, ev_next);
    }
    ev->ev_queued = 0;
    OS_EXIT_CRITICAL(sr);
}
