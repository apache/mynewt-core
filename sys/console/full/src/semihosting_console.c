/*
 * Copyright 2020 Casper Meijn <casper@meijn.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "os/mynewt.h"

#if MYNEWT_VAL(CONSOLE_SEMIHOSTING)

#include "semihosting/mbed_semihost_api.h"
#include <unistd.h>
#include "console_priv.h"

static unsigned char semihosting_tx_buffer[MYNEWT_VAL(CONSOLE_SEMIHOSTING_TX_BUF_SIZE)];
static unsigned char *semihosting_tx_buffer_pos = semihosting_tx_buffer;

static void
semihosting_console_flush(void)
{
    unsigned int size = semihosting_tx_buffer_pos - semihosting_tx_buffer;
    semihost_write(STDOUT_FILENO, semihosting_tx_buffer, size, 0);
    semihosting_tx_buffer_pos = semihosting_tx_buffer;
}

static void
semihosting_console_flush_event(struct os_event *ev)
{
    semihosting_console_flush();
}

static void
semihosting_console_write(unsigned char c)
{
    *semihosting_tx_buffer_pos = c;
    semihosting_tx_buffer_pos++;
    if (semihosting_tx_buffer_pos >= semihosting_tx_buffer + sizeof(semihosting_tx_buffer)) {
        semihosting_console_flush();
    }
}

static struct os_event semihosting_console_flush_ev = {
    .ev_cb = semihosting_console_flush_event,
};

int
console_out_nolock(int character)
{
    unsigned char c = (char)character;

    if (semihost_connected()) {
        semihosting_console_write(c);

        if (!OS_EVENT_QUEUED(&semihosting_console_flush_ev)) {
            os_eventq_put(os_eventq_dflt_get(), &semihosting_console_flush_ev);
        }
    }

    return character;
}

int
semihosting_console_is_init(void)
{
    return 1;
}

#endif
