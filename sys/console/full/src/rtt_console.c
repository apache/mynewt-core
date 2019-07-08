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

#if MYNEWT_VAL(CONSOLE_RTT_RETRY_COUNT) > 0

static void
rtt_console_wait_for_retry(void)
{
    uint32_t ticks;

    if (os_arch_in_isr()) {
#if MYNEWT_VAL(CONSOLE_RTT_RETRY_IN_ISR)
        os_cputime_delay_usecs(MYNEWT_VAL(CONSOLE_RTT_RETRY_DELAY_MS) * 1000);
#endif
    } else {
        ticks = max(1, os_time_ms_to_ticks32(MYNEWT_VAL(CONSOLE_RTT_RETRY_DELAY_MS)));
        os_time_delay(ticks);
    }
}

static void
rtt_console_write_ch(char c)
{
    static int rtt_console_retries_left = MYNEWT_VAL(CONSOLE_RTT_RETRY_COUNT);
    os_sr_t sr;
    int ret;

    while (1) {
        OS_ENTER_CRITICAL(sr);
        ret = SEGGER_RTT_WriteNoLock(0, &c, 1);
        OS_EXIT_CRITICAL(sr);

        /*
         * In case write failed we can wait a bit and retry to allow host pull
         * some data from buffer. However, in case there is no host connected
         * we should not spend time retrying over and over again so need to be
         * smarter here:
         * - each successful write resets retry counter to predefined value
         * - each failed write will retry until success or retry counter drops
         *   to zero
         * This means that if we failed to write some character after number of
         * retries (which means that most likely there is no host connected to
         * read data), we stop retrying until successful write (which means that
         * host is reading data).
         */

        if (ret) {
            rtt_console_retries_left = MYNEWT_VAL(CONSOLE_RTT_RETRY_COUNT);
            break;
        }

        if (rtt_console_retries_left <= 0) {
            break;
        }

        rtt_console_wait_for_retry();
        rtt_console_retries_left--;
    }
}

#else

static void
rtt_console_write_ch(char c)
{
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    SEGGER_RTT_WriteNoLock(0, &c, 1);
    OS_EXIT_CRITICAL(sr);
}

#endif

int
console_out_nolock(int character)
{
    char c = (char)character;

    if (g_console_silence) {
        return c;
    }

    if ('\n' == c) {
        rtt_console_write_ch('\r');
        console_is_midline = 0;
    } else {
        console_is_midline = 1;
    }

    rtt_console_write_ch(c);

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
