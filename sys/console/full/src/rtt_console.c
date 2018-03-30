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

#if MYNEWT_VAL(CONSOLE_INPUT)
void
rtt(void *arg)
{
    int key;
    int i = 0;
    uint32_t timeout;

    key = SEGGER_RTT_GetKey();
    if (key >= 0) {
        console_handle_char((char)key);
        i = 0;
    }
    /* These values were selected to keep the shell responsive
     * and at the same time reduce context switches.
     * Min sleep is 50ms and max is 250ms.
     */
    if (i < 5) {
        ++i;
    }
    timeout = 50000 * i;
    os_cputime_timer_relative(&rtt_timer, timeout);
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
    os_cputime_timer_init(&rtt_timer, rtt, NULL);
    /* start after a second */
    os_cputime_timer_relative(&rtt_timer, 1000000);
#endif
    return 0;
}

#endif /* MYNEWT_VAL(CONSOLE_RTT) */
