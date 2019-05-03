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

#ifndef _OS_ARCH_COMMON_H
#define _OS_ARCH_COMMON_H

#include <stdint.h>
#include "os/os_error.h"

#ifdef __cplusplus
extern "C" {
#endif

struct os_stack;
struct os_task;

#ifndef OS_STACK_PATTERN
#define OS_STACK_PATTERN                (0xdeadbeef)
#endif

#ifndef OS_ALIGNMENT
#define OS_ALIGNMENT                    (4)
#endif

#ifndef OS_STACK_ALIGNMENT
#define OS_STACK_ALIGNMENT              (8)
#endif

#ifndef OS_STACK_ALIGN
#define OS_STACK_ALIGN(__len)           (OS_ALIGN((__len), OS_STACK_ALIGNMENT))
#endif

#ifndef OS_ENTER_CRITICAL
#define OS_ENTER_CRITICAL(__os_sr)      (__os_sr = os_arch_save_sr())
#endif

#ifndef OS_EXIT_CRITICAL
#define OS_EXIT_CRITICAL(__os_sr)       (os_arch_restore_sr(__os_sr))
#endif

#ifndef OS_ASSERT_CRITICAL
#define OS_ASSERT_CRITICAL()            (assert(os_arch_in_critical()))
#endif

os_stack_t *os_arch_task_stack_init(struct os_task *, os_stack_t *, int);
void os_arch_ctx_sw(struct os_task *);
os_sr_t os_arch_save_sr(void);
void os_arch_restore_sr(os_sr_t);
int os_arch_in_critical(void);
void os_arch_init(void);
uint32_t os_arch_start(void);
os_error_t os_arch_os_init(void);
os_error_t os_arch_os_start(void);
void os_set_env(os_stack_t *);
void os_arch_init_task_stack(os_stack_t *sf);
void os_default_irq_asm(void);
void os_assert_cb(void);

#ifdef __cplusplus
}
#endif

#endif /* _OS_ARCH_COMMON_H */
