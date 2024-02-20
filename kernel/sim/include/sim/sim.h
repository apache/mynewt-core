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

#ifndef H_KERNEL_SIM_
#define H_KERNEL_SIM_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <setjmp.h>
#include "os/mynewt.h"
struct os_task;
struct stack_frame;

struct stack_frame {
    uintptr_t sf_mainsp;              /* stack on which main() is executing */
    sigjmp_buf sf_jb;
    struct os_task *sf_task;
};

void sim_task_start(struct stack_frame *sf, int rc);
os_stack_t *sim_task_stack_init(struct os_task *t, os_stack_t *stack_top,
                                int size);
os_error_t sim_os_start(void);
void sim_os_stop(void);
os_error_t sim_os_init(void);
void sim_ctx_sw(struct os_task *next_t);
os_sr_t sim_save_sr(void);
void sim_restore_sr(os_sr_t osr);
int sim_in_critical(void);
void sim_tick_idle(os_time_t ticks);

/**
 * Prints information about a crash to stdout.  This functionality is defined
 * as a macro rather than a function to ensure that it gets inlined, enforcing
 * a predictable call stack.
 */
#define OS_PRINT_ASSERT_SIM(file, line, func, e) do                         \
{                                                                           \
    if (!(file)) {                                                          \
        dprintf(1, "Assert @ 0x%lx\n",                                      \
                (long unsigned int)__builtin_return_address(0));            \
    } else {                                                                \
        dprintf(1, "Assert @ 0x%lx - %s:%d\n",                              \
                (long unsigned int)__builtin_return_address(0),             \
                (file), (line));                                            \
    }                                                                       \
} while (0)

#ifdef __cplusplus
}
#endif

#endif
