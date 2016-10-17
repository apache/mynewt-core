/**
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
#ifndef _MUTEX_TEST_H
#define _MUTEX_TEST_H

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include "sysinit/sysinit.h"
#include "testutil/testutil.h"
#include "os/os.h"
#include "os/os_test.h"
#include "os_test_priv.h"

#ifdef __cplusplus
#extern "C" {
#endif

#ifdef ARCH_sim
#define MUTEX_TEST_STACK_SIZE   1024
#else
#define MUTEX_TEST_STACK_SIZE   256
#endif

extern struct os_task task14;
extern os_stack_t stack14[OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE)];

extern struct os_task task15;
extern os_stack_t stack15[OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE)];

extern struct os_task task16;
extern os_stack_t stack16[OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE)];

extern struct os_task task17;
extern os_stack_t stack17[OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE)];

#define TASK14_PRIO (4)
#define TASK15_PRIO (5)
#define TASK16_PRIO (6)
#define TASK17_PRIO (7)

extern volatile int g_task14_val;
extern volatile int g_task15_val;
extern volatile int g_task16_val;
extern volatile int g_task17_val;
extern struct os_mutex g_mutex1;
extern struct os_mutex g_mutex2;
extern volatile int g_mutex_test;

void mutex_test_basic_handler(void *arg);
void mutex_test1_task14_handler(void *arg);
void mutex_test2_task14_handler(void *arg);
void task15_handler(void *arg);
void task16_handler(void *arg);
void task17_handler(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _MUTEX_TEST_H */
