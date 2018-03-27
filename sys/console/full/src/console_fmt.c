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
#include <stdarg.h>
#include <stdio.h>
#include "os/mynewt.h"
#include "console/console.h"
#include "console/ticks.h"

#define CONS_OUTPUT_MAX_LINE    128


#if MYNEWT_VAL(BASELIBC_PRESENT)

/**
 * Prints the specified format string to the console.
 *
 * @return                      The number of characters that would have been
 *                                  printed if the console buffer were
 *                                  unlimited.  This return value is analogous
 *                                  to that of snprintf.
 */
int
console_printf(const char *fmt, ...)
{
    va_list args;
    int num_chars;

    num_chars = 0;

    if (console_get_ticks()) {
        /* Prefix each line with a timestamp. */
        if (!console_is_midline) {
            num_chars += printf("%06lu ", (unsigned long)os_time_get());
        }
    }

    va_start(args, fmt);
    num_chars += vprintf(fmt, args);
    va_end(args);

    return num_chars;
}


#else

/**
 * Prints the specified format string to the console.
 *
 * @return                      The number of characters that would have been
 *                                  printed if the console buffer were
 *                                  unlimited.  This return value is analogous
 *                                  to that of snprintf.
 */
int
console_printf(const char *fmt, ...)
{
    va_list args;
    char buf[CONS_OUTPUT_MAX_LINE];
    int num_chars;
    int len;

    num_chars = 0;

    if (console_get_ticks()) {
        /* Prefix each line with a timestamp. */
        if (!console_is_midline) {
            len = snprintf(buf, sizeof(buf), "%06lu ",
                           (unsigned long)os_time_get());
            num_chars += len;
            console_write(buf, len);
        }
    }

    va_start(args, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    num_chars += len;
    if (len >= sizeof(buf)) {
        len = sizeof(buf) - 1;
    }
    console_write(buf, len);
    va_end(args);

    return num_chars;
}
#endif
