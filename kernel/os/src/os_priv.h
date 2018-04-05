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

#ifndef H_OS_PRIV_
#define H_OS_PRIV_

#include "os/mynewt.h"
#include "console/console.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct os_task g_idle_task;
extern struct os_task_list g_os_run_list;
extern struct os_task_list g_os_sleep_list;
extern struct os_task_stailq g_os_task_list;
extern struct os_callout_list g_callout_list;

void os_msys_init(void);

/**
 * Prints information about a crash to the console.  This functionality is
 * defined as a macro rather than a function to ensure that it gets inlined,
 * enforcing a predictable call stack.
 */
#define OS_PRINT_ASSERT(file, line, func, e) do                             \
{                                                                           \
    if (!(file)) {                                                          \
        console_printf("Assert @ 0x%x\n",                                   \
                       (unsigned int)__builtin_return_address(0));          \
    } else {                                                                \
        console_printf("Assert @ 0x%x - %s:%d\n",                           \
                       (unsigned int)__builtin_return_address(0),           \
                       (file), (line));                                     \
    }                                                                       \
} while (0)

#ifdef __cplusplus
}
#endif

#endif
