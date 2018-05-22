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

#include <assert.h>
#include "os/mynewt.h"

CTASSERT(sizeof(os_time_t) == 4);

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

os_time_t
os_time_get(void)
{
    return (g_os_time);
}

#if MYNEWT_VAL(OS_SCHEDULING)
static void
os_time_tick(int ticks)
{
    os_sr_t sr;
    os_time_t delta, prev_os_time;

    assert(ticks >= 0);

    OS_ENTER_CRITICAL(sr);
    prev_os_time = g_os_time;
    g_os_time += ticks;

    /*
     * Update 'basetod' when 'g_os_time' crosses the 0x00000000 and
     * 0x80000000 thresholds.
     */
    if ((prev_os_time ^ g_os_time) >> 31) {
        delta = g_os_time - basetod.ostime;
        os_deltatime(delta, &basetod.uptime, &basetod.uptime);
        os_deltatime(delta, &basetod.utctime, &basetod.utctime);
        basetod.ostime = g_os_time;
    }
    OS_EXIT_CRITICAL(sr);
}

void
os_time_advance(int ticks)
{
    assert(ticks >= 0);

    if (ticks > 0) {
        if (!os_started()) {
            g_os_time += ticks;
        } else {
            os_time_tick(ticks);
            os_callout_tick();
            os_sched_os_timer_exp();
            os_sched(NULL);
        }
    }
}
#else
void
os_time_advance(int ticks)
{
    g_os_time += ticks;
}
#endif

void
os_time_delay(int32_t osticks)
{
    os_sr_t sr;

    if (osticks > 0) {
        OS_ENTER_CRITICAL(sr);
        os_sched_sleep(os_sched_get_current_task(), (os_time_t)osticks);
        OS_EXIT_CRITICAL(sr);
        os_sched(NULL);
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

void
os_get_uptime(struct os_timeval *tvp)
{
  struct os_timeval tv;
  os_time_t delta;
  os_sr_t sr;
  os_time_t ostime;

  OS_ENTER_CRITICAL(sr);
  tv = basetod.uptime;
  ostime = basetod.ostime;
  delta = os_time_get() - ostime;
  OS_EXIT_CRITICAL(sr);

  os_deltatime(delta, &tv, tvp);
}

int64_t
os_get_uptime_usec(void)
{
  struct os_timeval tv;

  os_get_uptime(&tv);

  return (tv.tv_sec * 1000000 + tv.tv_usec);
}

int
os_time_ms_to_ticks(uint32_t ms, uint32_t *out_ticks)
{
    uint64_t ticks;

#if OS_TICKS_PER_SEC == 1000
    *out_ticks = ms;
    return 0;
#endif

    _Static_assert(OS_TICKS_PER_SEC <= UINT32_MAX,
                   "OS_TICKS_PER_SEC must be <= UINT32_MAX");

    ticks = ((uint64_t)ms * OS_TICKS_PER_SEC) / 1000;
    if (ticks > UINT32_MAX) {
        return OS_EINVAL;
    }

    *out_ticks = ticks;
    return 0;
}

int
os_time_ticks_to_ms(uint32_t ticks, uint32_t *out_ms)
{
    uint64_t ms;

#if OS_TICKS_PER_SEC == 1000
    *out_ms = ticks;
    return 0;
#endif

    ms = ((uint64_t)ticks * 1000) / OS_TICKS_PER_SEC;
    if (ms > UINT32_MAX) {
        return OS_EINVAL;
    }

    *out_ms = ms;

    return 0;
}

uint32_t
os_time_ms_to_ticks32(uint32_t ms)
{
#if OS_TICKS_PER_SEC == 1000
    return ms;
#else
    return ((uint64_t)ms * OS_TICKS_PER_SEC) / 1000;
#endif
}

uint32_t
os_time_ticks_to_ms32(uint32_t ticks)
{
#if OS_TICKS_PER_SEC == 1000
    return ticks;
#else
    return ((uint64_t)ticks * 1000) / OS_TICKS_PER_SEC;
#endif
}
