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
#include "os_test_priv.h"

TEST_CASE(os_mutex_test_basic)
{
    sysinit();

    os_mutex_init(&g_mutex1);

    os_task_init(&task14, "task14", mutex_test_basic_handler, NULL,
                 TASK14_PRIO, OS_WAIT_FOREVER, stack14,
                 OS_STACK_ALIGN(MUTEX_TEST_STACK_SIZE));

    os_start();
}
