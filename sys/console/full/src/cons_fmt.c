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
#include "syscfg/syscfg.h"
#include "console/console.h"
#include "os/os_time.h"

#define CONS_OUTPUT_MAX_LINE	128

#if MYNEWT_VAL(BASELIBC_PRESENT)
size_t console_file_write(FILE *p, const char *str, size_t cnt);

static const struct File_methods console_file_ops = {
    .write = console_file_write,
    .read = NULL
};

static const FILE console_file = {
    .vmt = &console_file_ops
};

void
console_printf(const char *fmt, ...)
{
    va_list args;

    /* Prefix each line with a timestamp. */
    if (!console_is_midline) {
        fprintf((FILE *)&console_file, "%lu:", (unsigned long)os_time_get());
    }

    va_start(args, fmt);
    vfprintf((FILE *)&console_file, fmt, args);
    va_end(args);
}

#else

void
console_printf(const char *fmt, ...)
{
    va_list args;
    char buf[CONS_OUTPUT_MAX_LINE];
    int len;

    /* Prefix each line with a timestamp. */
    if (!console_is_midline) {
        len = snprintf(buf, sizeof(buf), "%lu:", (unsigned long)os_time_get());
        console_write(buf, len);
    }

    va_start(args, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len >= sizeof(buf)) {
        len = sizeof(buf) - 1;
    }
    console_write(buf, len);
    va_end(args);
}
#endif
