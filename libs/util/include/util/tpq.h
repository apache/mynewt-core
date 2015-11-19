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
#ifndef __UTIL_TPQ_H__ 
#define __UTIL_TPQ_H__

#include <os/queue.h>
#include <os/os_eventq.h>

/* A task packet queue element */
struct tpq_elem {
    STAILQ_ENTRY(tpq_elem) tpq_next;
};

/* The task packet queue object */
struct tpq
{
    STAILQ_HEAD(, tpq_elem) tpq_head;
    struct os_event tpq_ev;
};

/**
 * Put an element on a task packet queue and post an event to an event queue. 
 * 
 * @param evq   Pointer to event queue
 * @param tpq   Pointer to task packet queue
 * @param elem  Pointer to element to enqueue
 */
void tpq_put(struct os_eventq *evq, struct tpq *tpq, struct tpq_elem *elem);

/**
 * Retrieve an element from a task packet queue. This removes the element at 
 * the head of the queue. 
 * 
 * @param head 
 * 
 * @return struct tpq_elem* 
 */
struct tpq_elem *tpq_get(struct tpq *tpq);

/**
 * Initialize a task packet queue 
 * 
 * @param tpq Pointer to task packet queue
 * @param ev_type Type of event.
 * @param ev_arg Argument of event
 * 
 * @return int 
 */
void tpq_init(struct tpq *tpq, uint8_t ev_type, void *ev_arg);

#endif /* __UTIL_TPQ_H__ */
