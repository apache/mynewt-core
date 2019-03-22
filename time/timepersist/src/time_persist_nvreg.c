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

#include <os/mynewt.h>

#if MYNEWT_VAL(TIMEPERSIST_NVREG_INDEX) >= 0

#include <hal/hal_nvreg.h>

static struct os_callout timepersist_timer;

/*
 * Note that not all MCUs support this size writes.
 * System uses 64bit value as seconds since 1970, while most MCUs will
 * have 32bit non-volatile registers (and some 8bit).
 * With 32 bits storage you get time pretty far in to the future.
 * Also, not storing timezone at the moment.
 */
void
timepersist(void)
{
    struct os_timeval tv;

    if (os_time_is_set()) {
        os_gettimeofday(&tv, NULL);
        hal_nvreg_write(MYNEWT_VAL(TIMEPERSIST_NVREG_INDEX), tv.tv_sec);
    }
}

static void
timepersist_tmo(struct os_event *ev)
{
    timepersist();
    os_callout_reset(&timepersist_timer,
                     MYNEWT_VAL(TIMEPERSIST_FREQ) * OS_TICKS_PER_SEC);
}

/*
 * Periodically store system wallclock to non-volatile storage.
 */
void
timepersist_init(void)
{
    struct os_timeval tv;

    tv.tv_sec = hal_nvreg_read(MYNEWT_VAL(TIMEPERSIST_NVREG_INDEX));
    if (tv.tv_sec != 0) {
        tv.tv_usec = 0;
        os_settimeofday(&tv, NULL);
    }
    os_callout_init(&timepersist_timer, os_eventq_dflt_get(), timepersist_tmo,
                    NULL);
    os_callout_reset(&timepersist_timer,
                     MYNEWT_VAL(TIMEPERSIST_FREQ) * OS_TICKS_PER_SEC);
}

#endif
