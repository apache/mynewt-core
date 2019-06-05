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

#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <inttypes.h>
#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

struct os_eventq;

struct console_input {
    char line[MYNEWT_VAL(CONSOLE_MAX_INPUT_LEN)];
};

typedef void (*console_rx_cb)(void);

int console_init(console_rx_cb rx_cb);
int console_is_init(void);

void console_write(const char *str, int cnt);
#if MYNEWT_VAL(CONSOLE_COMPAT)
int console_read(char *str, int cnt, int *newline);
#endif

void console_blocking_mode(void);
void console_non_blocking_mode(void);
void console_echo(int on);

static int inline
console_vprintf(const char *fmt, va_list ap)
{
    return 0;
}

static int console_printf(const char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));;
static int inline
console_printf(const char *fmt, ...)
{
    return 0;
}

void console_set_queues(struct os_eventq *avail_queue,
                        struct os_eventq *cmd_queue);

static void inline
console_set_completion_cb(uint8_t (*completion)(char *str, uint8_t len))
{
}

/**
 * Global indicating whether console is silent or not
 */
extern bool g_console_silence;
/**
 * Global indicating whether non nlip console output is silent or not
 */
extern bool g_console_silence_non_nlip;
/**
 * Global indicating whether non nlip console input is disabled or not
 */
extern bool g_console_ignore_non_nlip;

/**
 * Silences console output, input is still active
 *
 * @param silent Let console know if it needs to be silent,
 *        true for silence, false otherwise
 */
static void inline
console_silence(bool silent)
{
    g_console_silence = silent;
}

int console_handle_char(uint8_t byte);

/**
 * Silences non nlip console output, input is still active
 *
 * @param silent Let non nlip console know if it needs to be silent,
 *        true for silence, false otherwise
 */
static void inline
console_silence_non_nlip(bool silent)
{
    g_console_silence_non_nlip = silent;
}


/**
 * Ignores console input that is non nlip, output is still active
 *
 * @param ignore Lets console know if non nlip input should be disabled,
 *        true for ignore input, false otherwise
 */
static void inline
console_ignore_non_nlip(bool ignore)
{
    g_console_ignore_non_nlip = ignore;
}


extern int console_is_midline;
extern int console_out(int character);

int console_lock(int timeout);
int console_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* __CONSOLE_H__ */
