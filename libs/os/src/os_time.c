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
#include "os/queue.h"

#define OS_USEC_PER_TICK    (1000000 / OS_TICKS_PER_SEC)

os_time_t g_os_time;

/*
 * Time-of-day collateral.
 */
static struct {
    os_time_t ostime;
    struct os_timeval uptime;
    struct os_timeval utctime;
    struct os_timezone timezone;
} basetod;

static void
os_deltatime(os_time_t delta, const struct os_timeval *base,
    struct os_timeval *result)
{
    struct os_timeval tvdelta;

    tvdelta.tv_sec = delta / OS_TICKS_PER_SEC;
    tvdelta.tv_usec = (delta % OS_TICKS_PER_SEC) * OS_USEC_PER_TICK;
    os_timeradd(base, &tvdelta, result);
}

/*
 * Time-of-day collateral.
 */
static struct {
    os_time_t ostime;
    struct os_timeval uptime;
    struct os_timeval utctime;
    struct os_timezone timezone;
} basetod;

static void
os_deltatime(os_time_t delta, const struct os_timeval *base,
    struct os_timeval *result)
{
    struct os_timeval tvdelta;

    tvdelta.tv_sec = delta / OS_TICKS_PER_SEC;
    tvdelta.tv_usec = (delta % OS_TICKS_PER_SEC) * OS_USEC_PER_TICK;
    os_timeradd(base, &tvdelta, result);
}

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
    os_time_t delta;

    OS_ENTER_CRITICAL(sr);
    ++g_os_time;

    /*
     * Update 'basetod' when the lowest 31 bits of 'g_os_time' are all zero,
     * i.e. at 0x00000000 and 0x80000000.
     */
    if ((g_os_time << 1) == 0) {        /* XXX use __unlikely() here */
        delta = g_os_time - basetod.ostime;
        os_deltatime(delta, &basetod.uptime, &basetod.uptime);
        os_deltatime(delta, &basetod.utctime, &basetod.utctime);
        basetod.ostime = g_os_time;
    }
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

int
os_settimeofday(struct os_timeval *utctime, struct os_timezone *tz)
{
    os_sr_t sr;
    os_time_t delta;

    OS_ENTER_CRITICAL(sr);
    if (utctime != NULL) {
        /*
         * Update all time-of-day base values.
         */
        delta = os_time_get() - basetod.ostime;
        os_deltatime(delta, &basetod.uptime, &basetod.uptime);
        basetod.utctime = *utctime;
        basetod.ostime += delta;
    }

    if (tz != NULL) {
        basetod.timezone = *tz;
    }
    OS_EXIT_CRITICAL(sr);

    return (0);
}

int
os_gettimeofday(struct os_timeval *tv, struct os_timezone *tz)
{
    os_sr_t sr;
    os_time_t delta;

    OS_ENTER_CRITICAL(sr);
    if (tv != NULL) {
        delta = os_time_get() - basetod.ostime;
        os_deltatime(delta, &basetod.utctime, tv);
    }

    if (tz != NULL) {
        *tz = basetod.timezone;
    }
    OS_EXIT_CRITICAL(sr);

    return (0);
}
