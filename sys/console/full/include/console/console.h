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

typedef void (*console_rx_cb)(void);

int console_init(console_rx_cb rx_cb);
int console_is_init(void);
void console_write(const char *str, int cnt);
int console_read(char *str, int cnt, int *newline);
void console_blocking_mode(void);
void console_echo(int on);

void console_printf(const char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));;

extern int console_is_midline;

#ifdef __cplusplus
}
#endif

#endif /* __CONSOLE_H__ */
