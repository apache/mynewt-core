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
#include "util/tpq.h"

/**
 * Put an element on a task packet queue and post an event to an event queue. 
 * 
 * @param evq   Pointer to event queue
 * @param tpq   Pointer to task packet queue
 * @param elem  Pointer to element to enqueue
 */
void
tpq_put(struct os_eventq *evq, struct tpq *tpq, struct tpq_elem *elem)
{
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    STAILQ_INSERT_TAIL(&tpq->tpq_head, elem, tpq_next);
    OS_EXIT_CRITICAL(sr);
    os_eventq_put(evq, &tpq->tpq_ev);
}

/**
 * Retrieve an element from a task packet queue. This removes the element at 
 * the head of the queue. 
 * 
 * @param head 
 * 
 * @return struct tpq_elem* 
 */
struct tpq_elem *
tpq_get(struct tpq *tpq)
{
    os_sr_t sr;
    struct tpq_elem *elem;

    OS_ENTER_CRITICAL(sr);
    elem = STAILQ_FIRST(&tpq->tpq_head);
    if (elem) {
        STAILQ_REMOVE_HEAD(&tpq->tpq_head, tpq_next);
    }
    OS_EXIT_CRITICAL(sr);

    return elem;
}

/**
 * Initialize a task packet queue 
 * 
 * @param tpq Pointer to task packet queue
 * @param ev_type Type of event.
 * @param ev_arg Argument of event
 * 
 * @return int 
 */
void
tpq_init(struct tpq *tpq, uint8_t ev_type, void *ev_arg)
{
    struct os_event *ev;

    /* Initialize the task packet queue */
    STAILQ_INIT(&tpq->tpq_head);

    /* Initial task packet queue event */
    ev = &tpq->tpq_ev;
    ev->ev_arg = ev_arg;
    ev->ev_type = ev_type;
    ev->ev_queued = 0;
    STAILQ_NEXT(ev, ev_next) = NULL;
}

