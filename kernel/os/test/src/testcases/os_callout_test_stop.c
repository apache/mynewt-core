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

/* Test case of the callout_task_stop */
TEST_CASE_SELF(callout_test_stop)
{
    int k;

    /* Initialize the sending task */
    os_task_init(&callout_task_struct_stop_send, "callout_task_stop_send",
        callout_task_stop_send, NULL, SEND_STOP_CALLOUT_TASK_PRIO,
        OS_WAIT_FOREVER, callout_task_stack_stop_send, CALLOUT_STACK_SIZE);

    /* Initialize the receiving task */
    os_task_init(&callout_task_struct_stop_receive,
        "callout_task_stop_receive", callout_task_stop_receive, NULL,
        RECEIVE_STOP_CALLOUT_TASK_PRIO, OS_WAIT_FOREVER,
        callout_task_stack_stop_receive, CALLOUT_STACK_SIZE);

    for(k = 0; k < MULTI_SIZE; k++){
        os_eventq_init(&callout_stop_evq[k]);
    }

    /* Initialize the callout function */
    for (k = 0; k < MULTI_SIZE; k++){
        os_callout_init(&callout_stop_test[k], &callout_stop_evq[k],
           my_callout_stop_func, NULL);
    }
}
