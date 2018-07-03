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

#include "os/mynewt.h"

#if MYNEWT_VAL(CONSOLE_RTT)
#include <ctype.h>

#include "rtt/SEGGER_RTT.h"
#include "console/console.h"
#include "console_priv.h"

#if MYNEWT_VAL(CONSOLE_INPUT)
static struct hal_timer rtt_timer;
#endif

static const char CR = '\r';

int
console_out(int character)
{
    char c = (char)character;

    if ('\n' == c) {
        SEGGER_RTT_WriteWithOverwriteNoLock(0, &CR, 1);
        console_is_midline = 0;
    } else {
        console_is_midline = 1;
    }

    SEGGER_RTT_WriteWithOverwriteNoLock(0, &c, 1);

    return character;
}

void
console_rx_restart(void)
{
    os_cputime_timer_relative(&rtt_timer, 0);
}

#if MYNEWT_VAL(CONSOLE_INPUT)

#define RTT_INPUT_POLL_INTERVAL_MIN     10 /* ms */
#define RTT_INPUT_POLL_INTERVAL_STEP    10 /* ms */
#define RTT_INPUT_POLL_INTERVAL_MAX     MYNEWT_VAL(CONSOLE_RTT_INPUT_POLL_INTERVAL_MAX)

static void
rtt_console_poll_func(void *arg)
{
    static uint32_t itvl_ms = RTT_INPUT_POLL_INTERVAL_MIN;
    static int key = -1;
    int ret;

    if (key < 0) {
        key = SEGGER_RTT_GetKey();
    }

    if (key < 0) {
        itvl_ms += RTT_INPUT_POLL_INTERVAL_STEP;
        itvl_ms = min(itvl_ms, RTT_INPUT_POLL_INTERVAL_MAX);
    } else {
        while (key >= 0) {
            ret = console_handle_char((char)key);
            if (ret < 0) {
                return;
            }
            key = SEGGER_RTT_GetKey();
        }
        itvl_ms = RTT_INPUT_POLL_INTERVAL_MIN;
    }

    os_cputime_timer_relative(&rtt_timer, itvl_ms * 1000);
}
#endif

int
rtt_console_is_init(void)
{
    return 1;
}

int
rtt_console_init(void)
{
#if MYNEWT_VAL(CONSOLE_INPUT)
    os_cputime_timer_init(&rtt_timer, rtt_console_poll_func, NULL);
    /* start after a second */
    os_cputime_timer_relative(&rtt_timer, 1000000);
#endif
    return 0;
}

#endif /* MYNEWT_VAL(CONSOLE_RTT) */
