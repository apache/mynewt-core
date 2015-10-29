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

#include <console/console.h>
#include "os/os.h"

#include <unistd.h>

int             os_die_line;
const char     *os_die_module;

void __assert_func(const char *file, int line, const char *func, const char *e);

void
__assert_func(const char *file, int line, const char *func, const char *e)
{
    int sr;

    OS_ENTER_CRITICAL(sr);
    (void)sr;
    os_die_line = line;
    os_die_module = file;
    console_blocking_mode();
    console_printf("Assert '%s; failed in %s:%d\n", e, file, line);
    _exit(1);
}
