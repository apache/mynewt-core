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
#include "os_test_priv.h"

TEST_CASE(os_mutex_test_case_2)
{
    sysinit();

    g_mutex_test = 2;
    g_task14_val = 0;
    g_task15_val = 0;
    g_task16_val = 0;
    os_mutex_init(&g_mutex1);
    os_mutex_init(&g_mutex2);

    os_task_init(&task14, "task14", mutex_test2_task14_handler, NULL,
                 TASK14_PRIO, OS_WAIT_FOREVER, stack14,
                 OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE));

    os_task_init(&task15, "task15", task15_handler, NULL, TASK15_PRIO, 
            OS_WAIT_FOREVER, stack15, OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE));

    os_task_init(&task16, "task16", task16_handler, NULL, TASK16_PRIO, 
            OS_WAIT_FOREVER, stack16, OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE));

    os_task_init(&task17, "task17", task17_handler, NULL, TASK17_PRIO, 
            OS_WAIT_FOREVER, stack17, OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE));
 
    os_start();
}
