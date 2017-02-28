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

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

struct os_eventq;

#define MAX_LINE_LEN 80
struct console_input {
    char line[MAX_LINE_LEN];
};

int console_init(struct os_eventq *avail_queue, struct os_eventq *cmd_queue,
                 uint8_t (*completion)(char *str, uint8_t len));
size_t console_file_write(void *arg, const char *str, size_t cnt);
void console_write(const char *str, int cnt);
void console_printf(const char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));;
int console_handle_char(uint8_t byte);
int uart_console_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __CONSOLE_H__ */
