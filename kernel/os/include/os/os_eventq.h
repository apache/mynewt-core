/*
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


/**
 * @addtogroup OSKernel
 * @{
 *   @defgroup OSEvent Event Queues
 *   @{
 */

#ifndef _OS_EVENTQ_H
#define _OS_EVENTQ_H

#include <inttypes.h>
#include "os/os_time.h"
#include "os/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

struct os_event;
typedef void os_event_fn(struct os_event *ev);

/**
 * Structure representing an OS event.  OS events get placed onto the
 * event queues and are consumed by tasks.
 */
struct os_event {
    /** Whether this OS event is queued on an event queue. */
    uint8_t ev_queued;
    /**
     * Callback to call when the event is taken off of an event queue.
     * APIs, except for os_eventq_run(), assume this callback will be called by
     * the user.
     */
    os_event_fn *ev_cb;
    /** Argument to pass to the event queue callback. */
    void *ev_arg;


    STAILQ_ENTRY(os_event) ev_next;
};

/** Return whether or not the given event is queued. */
#define OS_EVENT_QUEUED(__ev) ((__ev)->ev_queued)

struct os_eventq {
    /** Pointer to task that "owns" this event queue. */
    struct os_task *evq_owner;
    /**
     * Pointer to the task that is sleeping on this event queue, either NULL,
     * or the owner task.
     */
    struct os_task *evq_task;


    STAILQ_HEAD(, os_event) evq_list;
};


/**
 * Initialize the event queue
 *
 * @param evq The event queue to initialize
 */
void os_eventq_init(struct os_eventq *);

/**
 * Check whether the event queue is initialized.
 *
 * @param evq The event queue to check
 */
int os_eventq_inited(const struct os_eventq *evq);

/**
 * Put an event on the event queue.
 *
 * @param evq The event queue to put an event on
 * @param ev The event to put on the queue
 */
void os_eventq_put(struct os_eventq *, struct os_event *);

/**
 * Poll an event from the event queue and return it immediately.
 * If no event is available, don't block, just return NULL.
 *
 * @return Event from the queue, or NULL if none available.
 */
struct os_event *os_eventq_get_no_wait(struct os_eventq *evq);

/**
 * Pull a single item from an event queue.  This function blocks until there
 * is an item on the event queue to read.
 *
 * @param evq The event queue to pull an event from
 *
 * @return The event from the queue
 */
struct os_event *os_eventq_get(struct os_eventq *);

/**
 * Pull a single item off the event queue and call it's event
 * callback.
 *
 * @param evq The event queue to pull the item off.
 */
void os_eventq_run(struct os_eventq *evq);


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
struct os_event *os_eventq_poll(struct os_eventq **, int, os_time_t);

/**
 * Remove an event from the queue.
 *
 * @param evq The event queue to remove the event from
 * @param ev  The event to remove from the queue
 */
void os_eventq_remove(struct os_eventq *, struct os_event *);

/**
 * Retrieves the default event queue processed by OS main task.
 *
 * @return                      The default event queue.
 */
struct os_eventq *os_eventq_dflt_get(void);

/**
 * @cond INTERNAL_HIDDEN
 * [DEPRECATED]
 */
void os_eventq_designate(struct os_eventq **dst, struct os_eventq *val,
                         struct os_event *start_ev);

/**
 * @endcond
 */

#ifdef __cplusplus
}
#endif

#endif /* _OS_EVENTQ_H */


/**
 *   @} OSEvent
 * @} OSKernel
 */
