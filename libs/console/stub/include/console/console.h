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
#ifndef __CONSOLE_H__
#define __CONSOLE_H__

typedef void (*console_rx_cb)(int full_line);

static int inline console_init(console_rx_cb rxcb)
{
    return 0;
}

static void inline console_write(char *str, int cnt)
{
}

static int inline console_read(char *str, int cnt)
{
    return 0;
}

static void inline console_printf(const char *fmt, ...)
{
    ;
}

#endif /* __CONSOLE__ */

