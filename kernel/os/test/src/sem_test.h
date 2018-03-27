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
#ifndef _SEM_TEST_H
#define  _SEM_TEST_H

#include <stdio.h>
#include <string.h>
#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "os_test_priv.h"

#ifdef __cplusplus
#extern "C" {
#endif

extern struct os_sem g_sem1;

const char *sem_test_sem_to_s(const struct os_sem *sem);
void sem_test_sleep_task_handler(void *arg);
void sem_test_pend_release_loop(int delay, int timeout, int itvl);
void sem_test_basic_handler(void *arg);
void sem_test_1_task1_handler(void *arg);
void sem_test_1_task2_handler(void *arg);
void sem_test_1_task3_handler(void *arg);
void sem_test_2_task2_handler(void *arg);
void sem_test_2_task3_handler(void *arg);
void sem_test_2_task4_handler(void *arg);
void sem_test_3_task2_handler(void *arg);
void sem_test_3_task3_handler(void *arg);
void sem_test_3_task4_handler(void *arg);
void sem_test_4_task2_handler(void *arg);
void sem_test_4_task3_handler(void *arg);
void sem_test_4_task4_handler(void *arg); 

#ifdef __cplusplus
}
#endif

#endif /*  _SEM_TEST_H */
