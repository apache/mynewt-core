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

/**
 * Initialize the event queue
 *
 * @param evq The event queue to initialize
 */
void
os_eventq_init(struct os_eventq *evq)
{
    memset(evq, 0, sizeof(*evq));
    STAILQ_INIT(&evq->evq_list);
}

/**
 * Put an event on the event queue.
 *
 * @param evq The event queue to put an event on 
 * @param ev The event to put on the queue
 */
void
os_eventq_put(struct os_eventq *evq, struct os_event *ev)
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

    resched = 0;
    if (evq->evq_task) {
        /* If task waiting on event, wake it up.
         * Check if task is sleeping, because another event 
         * queue may have woken this task up beforehand.
         */
        if (evq->evq_task->t_state == OS_TASK_SLEEP) {
            os_sched_wakeup(evq->evq_task);
            resched = 1;
        }
        /* Either way, NULL out the task, because the task will
         * be awake upon exit of this function.
         */
        evq->evq_task = NULL;
    }

    OS_EXIT_CRITICAL(sr);

    if (resched) {
        os_sched(NULL);
    }
}

/**
 * Pull a single item from an event queue.  This function blocks until there 
 * is an item on the event queue to read.
 *
 * @param evq The event queue to pull an event from
 *
 * @return The event from the queue
 */
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

        os_sched(NULL);

        OS_ENTER_CRITICAL(sr);
        evq->evq_task = NULL;
        goto pull_one;
    }
    OS_EXIT_CRITICAL(sr);

    return (ev);
}

/**
 * Poll the list of event queues specified by the evq parameter 
 * (size nevqs), and return the "first" event available on any of 
 * the queues.  Event queues are searched in the order that they 
 * are passed in the array.
 *
 * @param evq Array of event queues
 * @param nevqs Number of event queues in evq
 * @param timo Timeout, forever if OS_WAIT_FOREVER is passed to poll.
 *
 * @return An event, or NULL if no events available
 */
struct os_event *
os_eventq_poll(struct os_eventq **evq, int nevqs, os_time_t timo)
{
    struct os_event *ev;
    struct os_task *cur_t;
    int i, j;
    os_sr_t sr;

    ev = NULL;

    OS_ENTER_CRITICAL(sr);
    cur_t = os_sched_get_current_task();

    for (i = 0; i < nevqs; i++) {
        ev = STAILQ_FIRST(&evq[i]->evq_list);
        if (ev) {
            STAILQ_REMOVE(&evq[i]->evq_list, ev, os_event, ev_next);
            ev->ev_queued = 0;
            /* Reset the items that already have an evq task set
             */
            for (j = 0; j < i; j++) {
                evq[j]->evq_task = NULL;
            }

            OS_EXIT_CRITICAL(sr);
            goto has_event;
        }
        evq[i]->evq_task = cur_t;
    }

    os_sched_sleep(cur_t, timo);
    OS_EXIT_CRITICAL(sr);

    os_sched(NULL);

    OS_ENTER_CRITICAL(sr);
    for (i = 0; i < nevqs; i++) {
        /* Go through the entire loop to clear the evq_task variable, 
         * given this task is no longer sleeping on the event queues.
         * Return the first event found, so only grab the event if 
         * we haven't found one.
         */
        if (!ev) {
            ev = STAILQ_FIRST(&evq[i]->evq_list);
            if (ev) {
                STAILQ_REMOVE(&evq[i]->evq_list, ev, os_event, ev_next);
                ev->ev_queued = 0;
            }
        }
        evq[i]->evq_task = NULL;
    }
    OS_EXIT_CRITICAL(sr);

has_event:
    return (ev);
}

/**
 * Remove an event from the queue.
 *
 * @param evq The event queue to remove the event from
 * @param ev  The event to remove from the queue
 */
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
