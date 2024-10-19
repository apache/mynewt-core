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
#ifndef _OS_CALLOUT_H
#define _OS_CALLOUT_H

/**
 * @addtogroup OSKernel
 * @{
 *   @defgroup OSCallouts  OS Callouts
 *   @{
 */


#include "os/os_eventq.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Structure containing the definition of a callout, initialized
 * by os_callout_init() and passed to callout functions.
 */
struct os_callout {
    /** Event to post when the callout expires. */
    struct os_event c_ev;
    /** Pointer to the event queue to post the event to */
    struct os_eventq *c_evq;
    /** Number of ticks in the future to expire the callout */
    os_time_t c_ticks;

    /** Next callout in the list */
    TAILQ_ENTRY(os_callout) c_next;
};

/**
 * @cond INTERNAL_HIDDEN
 */

TAILQ_HEAD(os_callout_list, os_callout);

/**
 * @endcond
 */

/**
 * Initialize a callout.
 *
 * Callouts are used to schedule events in the future onto a task's event
 * queue.  Callout timers are scheduled using the os_callout_reset()
 * function.  When the timer expires, an event is posted to the event
 * queue specified in os_callout_init().  The event argument given here
 * is posted in the ev_arg field of that event.
 *
 * @param c                 The callout to initialize
 * @param evq               The event queue to post an OS_EVENT_T_TIMER event to
 * @param ev_cb             The function to call on this callout for the host
 *                              task used to provide multiple timer events to
 *                              a task (this can be NULL).
 * @param ev_arg            The argument to provide to the event when posting
 *                              the timer.
 */
void os_callout_init(struct os_callout *c, struct os_eventq *evq,
                     os_event_fn *ev_cb, void *ev_arg);


/**
 * Stop the callout from firing off, any pending events will be cleared.
 *
 * @param c                 The callout to stop
 */
void os_callout_stop(struct os_callout *c);


/**
 * Reset the callout to fire off in 'ticks' ticks.
 *
 * @param c                 The callout to reset
 * @param ticks             The number of ticks to wait before posting an event
 *
 * @return                  0 on success;
 *                          non-zero on failure
 */
int os_callout_reset(struct os_callout *c, os_time_t ticks);

/**
 * Returns the number of ticks which remains to callout.
 *
 * @param c                 The callout to check
 * @param now               The current time in OS ticks
 *
 * @return                  Number of ticks to first pending callout
 */
os_time_t os_callout_remaining_ticks(struct os_callout *c, os_time_t now);

/**
 * Returns whether the callout is pending or not.
 *
 * @param c                 The callout to check
 *
 * @return                  1 if queued;
 *                          0 if not queued.
 */
static inline int
os_callout_queued(struct os_callout *c)
{
    return c->c_next.tqe_prev != NULL;
}

/**
 * @cond INTERNAL_HIDDEN
 */

/**
 * This function is called by the OS in the time tick. It searches the list
 * of callouts, and sees if any of them are ready to run. If they are ready
 * to run, it posts an event for each callout that's ready to run,
 * to the event queue provided to os_callout_init().
 */
void os_callout_tick(void);

/**
 * Returns the number of ticks to the first pending callout. If there are no
 * pending callouts then return OS_TIMEOUT_NEVER instead.
 *
 * @param now               The time now
 *
 * @return                  Number of ticks to first pending callout
 */
os_time_t os_callout_wakeup_ticks(os_time_t now);

/**
 * @endcond
 */

#ifdef __cplusplus
}
#endif

#endif /* _OS_CALLOUT_H */

/**
 *   @} OSCallouts
 * @} OS Kernel
 */


