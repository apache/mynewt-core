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
#ifndef _MUTEX_TEST_H
#define _MUTEX_TEST_H

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "os_test_priv.h"

#ifdef __cplusplus
#extern "C" {
#endif

extern volatile int g_task1_val;
extern volatile int g_task2_val;
extern volatile int g_task3_val;
extern volatile int g_task4_val;
extern struct os_mutex g_mutex1;
extern struct os_mutex g_mutex2;
extern volatile int g_mutex_test;

void mutex_test_basic_handler(void *arg);
void mutex_test1_task1_handler(void *arg);
void mutex_test2_task1_handler(void *arg);
void mutex_task2_handler(void *arg);
void mutex_task3_handler(void *arg);
void mutex_task4_handler(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _MUTEX_TEST_H */
