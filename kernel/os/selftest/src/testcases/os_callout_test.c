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

/* Test case to test the basics of the callout */
TEST_CASE_SELF(callout_test)
{
    /* Initialize the sending task */
    os_task_init(&callout_task_struct_send, "callout_task_send",
        callout_task_send, NULL, SEND_CALLOUT_TASK_PRIO, OS_WAIT_FOREVER,
        callout_task_stack_send, CALLOUT_STACK_SIZE);

    /* Initialize the receive task */
    os_task_init(&callout_task_struct_receive, "callout_task_receive",
        callout_task_receive, NULL, RECEIVE_CALLOUT_TASK_PRIO, OS_WAIT_FOREVER,
        callout_task_stack_receive, CALLOUT_STACK_SIZE);

    os_eventq_init(&callout_evq);

    /* Initialize the callout function */
    os_callout_init(&callout_test_c, &callout_evq, my_callout, NULL);
}
