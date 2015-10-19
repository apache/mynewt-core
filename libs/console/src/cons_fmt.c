/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdarg.h>
#include <stdio.h>
#include <console/console.h>

#define CONS_OUTPUT_MAX_LINE	128

void console_printf(const char *fmt, ...)
{
    va_list args;
    char buf[CONS_OUTPUT_MAX_LINE];
    int len;

    va_start(args, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len >= sizeof(buf)) {
        len = sizeof(buf) - 1;
    }
    if (buf[len - 1] != '\n') {
        if (len != sizeof(buf) - 1) {
            len++;
        }
        buf[len - 1] = '\n';
    }
    console_write(buf, len);
}
