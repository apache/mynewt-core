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

#ifndef __TIMESCHED_H
#define __TIMESCHED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "os/os_eventq.h"
#include "os/os_time.h"
#include "os/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

struct timesched_timer {
    struct os_timeval expire;
    struct os_eventq *evq;
    struct os_event ev;

    TAILQ_ENTRY(timesched_timer) link;
};

/**
 * Initialize timer structure
 *
 * This function initializes a timer element with event queue and an event.
 * After timer is initialized it can be started with timesched_timer_start() to
 * expire at specified clock time. When this happens, an event will be sent to
 * event queue as initialized here.
 *
 * @param timer   Timer to initialize
 * @param evq     Event queue to post event on timer expire to
 * @param ev_cb   Callback of event associated with timer
 * @param ev_arg  Argument of event associated with timer
 */
void timesched_timer_init(struct timesched_timer *timer, struct os_eventq *evq,
                          os_event_fn *ev_cb, void *ev_arg);

/**
 * Start timer
 *
 * This function starts a timer to expire at specified clock time.
 *
 * @param timer    Timer to start
 * @param utctime  Time value (UTC) at which timer should expire
 *
 * @return OS_OK on success, error otherwise
 */
int timesched_timer_start(struct timesched_timer *timer,
                          struct os_timeval *utctime);

/**
 * Stop timer
 *
 * This function stops a timer from expiring. If given timer was not started,
 * this function has no effect.
 *
 * @param timer  Timer to stop
 *
 * @return OS_OK on success, error otherwise
 */
int timesched_timer_stop(struct timesched_timer *timer);

#ifdef __cplusplus
}
#endif

#endif  /* __TIMESCHED_H */
