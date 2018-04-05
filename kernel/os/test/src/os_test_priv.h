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

#ifndef H_OS_TEST_PRIV_
#define H_OS_TEST_PRIV_

#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "os_test_priv.h"

#include "callout_test.h"

#include "eventq_test.h"
#include "mbuf_test.h"
#include "mempool_test.h"
#include "mutex_test.h"
#include "sem_test.h"

#ifdef __cplusplus
extern "C" {
#endif

/* These test tasks must have a lower priority than the main task (i.e.,
 * greater number).  These tasks should not start running until the main task
 * has created all of them and goes idle.  The reason is that these tasks need
 * to interract with one another, so they require all sibling tasks to be
 * running.
 */
#define TASK1_PRIO (MYNEWT_VAL(OS_MAIN_TASK_PRIO) + 1)
#define TASK2_PRIO (TASK1_PRIO + 1)
#define TASK3_PRIO (TASK2_PRIO + 1)
#define TASK4_PRIO (TASK3_PRIO + 1)

extern struct os_task task1;
extern os_stack_t *stack1;
extern uint32_t stack1_size;

extern struct os_task task2;
extern os_stack_t *stack2;
extern uint32_t stack2_size;

extern struct os_task task3;
extern os_stack_t *stack3;
extern uint32_t stack3_size;

extern struct os_task task4;
extern os_stack_t *stack4;
extern uint32_t stack4_size;

void os_test_restart(void);

int os_mempool_test_suite(void);
int os_mbuf_test_suite(void);
int os_mutex_test_suite(void);
int os_sem_test_suite(void);
int os_eventq_test_suite(void);
int os_callout_test_suite(void);

#ifdef __cplusplus
}
#endif

#endif
