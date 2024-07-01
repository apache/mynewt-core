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

/*-
 * Copyright (c) 1982, 1986, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)time.h      8.5 (Berkeley) 5/4/95
 * $FreeBSD$
 */


/**
 * @addtogroup OSKernel
 * @{
 *   @defgroup OSTime Time
 *   @{
 */

#ifndef _OS_TIME_H
#define _OS_TIME_H

#include <stdbool.h>
#include <stdint.h>
#include "os/os_arch.h"
#include "os/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UINT32_MAX
/** Maximum value of 32-bit unsigned integer. */
#define UINT32_MAX  0xFFFFFFFFU
#endif

#ifndef INT32_MAX
/** Maximum value of 32-bit signed integer. */
#define INT32_MAX   0x7FFFFFFF
#endif

#ifdef OS_TICKS_PER_SEC
#warning "OS_TICKS_PER_SEC should be configured in syscfg"
#else
#if MYNEWT_VAL(OS_TICKS_PER_SEC)
#define OS_TICKS_PER_SEC        MYNEWT_VAL(OS_TICKS_PER_SEC)
#else
#error "Application, BSP or target must specify OS_TICKS_PER_SEC syscfg value"
#endif
#endif

/** Signed 32-bit system time type definition. */
typedef int32_t os_stime_t;

/** Unsigned 32-bit system time type definition. */
typedef uint32_t os_time_t;

/** Maximum value for os_time_t. */
#define OS_TIME_MAX UINT32_MAX

/** Maximum value for os_stime_t. */
#define OS_STIME_MAX INT32_MAX

/** Used to wait forever for events and mutexs */
#define OS_TIMEOUT_NEVER    (OS_TIME_MAX)


/**
 * Get the current OS time in ticks
 *
 * @return                      OS time in ticks
 */
os_time_t os_time_get(void);

/**
 * Move OS time forward ticks.
 *
 * @param ticks                 The number of ticks to move time forward.
 */
void os_time_advance(int ticks);

/**
 * Puts the current task to sleep for the specified number of os ticks. There
 * is no delay if ticks is 0.
 *
 * @param osticks               Number of ticks to delay (0 means no delay).
 */
void os_time_delay(os_time_t osticks);

/**
 * @defgroup OSTime_cmp_macros Helper macros for time comparisons
 * @{
 */

/**
 * Checks if time tick __t1 is less than time tick __t2.
 *
 * @param __t1                  The first time tick to compare.
 * @param __t2                  The second time tick to compare.
 *
 * @return                      True if __t1 is less than __t2;
 *                              false otherwise.
 */
#define OS_TIME_TICK_LT(__t1, __t2) ((os_stime_t) ((__t1) - (__t2)) < 0)

/**
 * Checks if time tick __t1 is greater than time tick __t2.
 *
 * @param __t1                  The first time tick to compare.
 * @param __t2                  The second time tick to compare.
 *
 * @return                      True if __t1 is greater than __t2;
 *                              false otherwise.
 */
#define OS_TIME_TICK_GT(__t1, __t2) ((os_stime_t) ((__t1) - (__t2)) > 0)

/**
 * Checks if time tick __t1 is greater than or equal to time tick __t2.
 *
 * @param __t1                  The first time tick to compare.
 * @param __t2                  The second time tick to compare.
 *
 * @return                      True if __t1 is greater than or equal to __t2;
 *                              false otherwise.
 */
#define OS_TIME_TICK_GEQ(__t1, __t2) ((os_stime_t) ((__t1) - (__t2)) >= 0)

/**
 * Checks if timeval __t1 is less than timeval __t2.
 *
 * @param __t1                  The first timeval to compare.
 * @param __t2                  The second timeval to compare.
 *
 * @return                      True if __t1 is less than __t2;
 *                              false otherwise.
 */
#define OS_TIMEVAL_LT(__t1, __t2) \
    (((__t1).tv_sec < (__t2).tv_sec) || \
     (((__t1).tv_sec == (__t2).tv_sec) && ((__t1).tv_usec < (__t2).tv_usec)))

/**
 * Checks if timeval __t1 is less than or equal to timeval __t2.
 *
 * @param __t1                  The first timeval to compare.
 * @param __t2                  The second timeval to compare.
 *
 * @return                      True if __t1 is less than or equal to __t2;
 *                              false otherwise.
 */
#define OS_TIMEVAL_LEQ(__t1, __t2) \
    (((__t1).tv_sec < (__t2).tv_sec) || \
     (((__t1).tv_sec == (__t2).tv_sec) && ((__t1).tv_usec <= (__t2).tv_usec)))

/**
 * Checks if timeval __t1 is greater than timeval __t2.
 *
 * @param __t1                  The first timeval to compare.
 * @param __t2                  The second timeval to compare.
 *
 * @return                      True if __t1 is greater than __t2;
 *                              false otherwise.
 */
#define OS_TIMEVAL_GT(__t1, __t2) \
    (((__t1).tv_sec > (__t2).tv_sec) || \
     (((__t1).tv_sec == (__t2).tv_sec) && ((__t1).tv_usec > (__t2).tv_usec)))

/**
 * Checks if timeval __t1 is greater than or equal to timeval __t2.
 *
 * @param __t1                  The first timeval to compare.
 * @param __t2                  The second timeval to compare.
 *
 * @return                      True if __t1 is greater than or equal to __t2;
 *                              false otherwise.
 */
#define OS_TIMEVAL_GEQ(__t1, __t2) \
    (((__t1).tv_sec > (__t2).tv_sec) || \
     (((__t1).tv_sec == (__t2).tv_sec) && ((__t1).tv_usec >= (__t2).tv_usec)))


/** @} */

/**
 * Structure representing time since Jan 1 1970 with microsecond
 * granularity
 */
struct os_timeval {
    /** Seconds */
    int64_t tv_sec;
    /** Microseconds within the second */
    int32_t tv_usec;
};

/** Structure representing a timezone offset */
struct os_timezone {
    /** Minutes west of GMT */
    int16_t tz_minuteswest;
    /** Daylight savings time correction (if any) */
    int16_t tz_dsttime;
};

/**
 * Represents a time change.  Passed to time change listeners when the current
 * time-of-day is set.
 */
struct os_time_change_info {
    /** UTC time prior to change. */
    const struct os_timeval *tci_prev_tv;
    /** Time zone prior to change. */
    const struct os_timezone *tci_prev_tz;
    /** UTC time after change. */
    const struct os_timeval *tci_cur_tv;
    /** Time zone after change. */
    const struct os_timezone *tci_cur_tz;
    /** True if the time was not set prior to change. */
    bool tci_newly_synced;
};

/**
 * Callback that is executed when the time-of-day is set.
 *
 * @param info                  Describes the time change that just occurred.
 * @param arg                   Optional argument correponding to listener.
 */
typedef void os_time_change_fn(const struct os_time_change_info *info,
                               void *arg);

/** Time change listener.  Notified when the time-of-day is set. */
struct os_time_change_listener {
    /** Callback invoked when the time-of-day is set. */
    os_time_change_fn *tcl_fn;
    /** Argument to be passed to the callback function. */
    void *tcl_arg;

    /** Next listener in the list. */
    STAILQ_ENTRY(os_time_change_listener) tcl_next;
};

/**
 * Add first two timeval arguments and place results in third timeval
 * argument.
 */
#define os_timeradd(tvp, uvp, vvp)                                      \
        do {                                                            \
                (vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;          \
                (vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;       \
                if ((vvp)->tv_usec >= 1000000) {                        \
                        (vvp)->tv_sec++;                                \
                        (vvp)->tv_usec -= 1000000;                      \
                }                                                       \
        } while (0)


/**
 * Subtract first two timeval arguments and place results in third timeval
 * argument.
 */
#define os_timersub(tvp, uvp, vvp)                                      \
        do {                                                            \
                (vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;          \
                (vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;       \
                if ((vvp)->tv_usec < 0) {                               \
                        (vvp)->tv_sec--;                                \
                        (vvp)->tv_usec += 1000000;                      \
                }                                                       \
        } while (0)


/**
 * Set the time of day.  This does not modify os time, but rather just modifies
 * the offset by which we are tracking real time against os time.  This
 * function notifies all registered time change listeners.
 *
 * @param utctime               A timeval representing the UTC time we are
 *                                  setting
 * @param tz                    The time-zone to apply against the utctime being
 *                                  set.
 *
 * @return                      0 on success;
 *                              non-zero on failure.
 */
int os_settimeofday(struct os_timeval *utctime, struct os_timezone *tz);

/**
 * Get the current time of day.  Returns the time of day in UTC
 * into the utctime argument, and returns the timezone (if set) into
 * tz.
 *
 * @param utctime               The structure to put the UTC time of day into
 * @param tz                    The structure to put the timezone information
 *                                  into
 *
 * @return 0 on success, non-zero on failure
 */
int os_gettimeofday(struct os_timeval *utctime, struct os_timezone *tz);

/**
 * Indicates whether the time has been set.
 *
 * @return                      True if time is set;
 *                              false otherwise.
 */
bool os_time_is_set(void);

/**
 * Get time since boot in microseconds.
 *
 * @return                      Time since boot in microseconds
 */
int64_t os_get_uptime_usec(void);

/**
 * Get time since boot as os_timeval.
 *
 * @param tvp                   Structure to put the time since boot.
 */
void os_get_uptime(struct os_timeval *tvp);

/**
 * Converts milliseconds to OS ticks.
 *
 * @param ms                    The milliseconds input.
 * @param out_ticks             The OS ticks output.
 *
 * @return                      0 on success;
 *                              OS_EINVAL if the result is too large to fit in
 *                                  a uint32_t.
 */
int os_time_ms_to_ticks(uint32_t ms, os_time_t *out_ticks);

/**
 * Converts OS ticks to milliseconds.
 *
 * @param ticks                 The OS ticks input.
 * @param out_ms                The milliseconds output.
 *
 * @return                      0 on success;
 *                              OS_EINVAL if the result is too large to fit in
 *                                  a uint32_t.
 */
int os_time_ticks_to_ms(os_time_t ticks, uint32_t *out_ms);


/**
 * Converts milliseconds to OS ticks.
 *
 * This function does not check if conversion overflows and should be only used
 * in cases where input is known to be small enough not to overflow.
 *
 * @param ms                    The milliseconds input.
 *
 * @return                      The number of OS ticks.
 */
static inline os_time_t
os_time_ms_to_ticks32(uint32_t ms)
{
#if OS_TICKS_PER_SEC == 1000
    return ms;
#else
    return ((uint64_t)ms * OS_TICKS_PER_SEC) / 1000;
#endif
}

/**
 * Converts OS ticks to milliseconds.
 *
 * This function does not check if conversion overflows and should be only used
 * in cases where input is known to be small enough not to overflow.
 *
 * @param ticks                 The OS ticks input.
 *
 * @return                      The number of milliseconds.
 */
static inline uint32_t
os_time_ticks_to_ms32(os_time_t ticks)
{
#if OS_TICKS_PER_SEC == 1000
    return ticks;
#else
    return ((uint64_t)ticks * 1000) / OS_TICKS_PER_SEC;
#endif
}

/**
 * Registers a time change listener.  Whenever the time is set, all registered
 * listeners are notified.  The provided pointer is added to an internal list,
 * so the listener's lifetime must extend indefinitely (or until the listener
 * is removed).
 *
 * @note This function is not thread safe.  The following operations must be
 * kept exclusive:
 *     o Addition of listener
 *     o Removal of listener
 *     o Setting time
 *
 * @param listener              The listener to register.
 */
void os_time_change_listen(struct os_time_change_listener *listener);

/**
 * Unregisters a time change listener.
 *
 * @note This function is not thread safe.  The following operations must be
 * kept exclusive:
 *     o Addition of listener
 *     o Removal of listener
 *     o Setting time
 *
 * @param listener              The listener to unregister.
 *
 * @return                      0 on success;
 *                              non-zero error code on failure
 */
int os_time_change_remove(const struct os_time_change_listener *listener);

#ifdef __cplusplus
}
#endif

#endif /* _OS_TIME_H */


/**
 *   @} OSKernel
 * @} OSTime
 */
