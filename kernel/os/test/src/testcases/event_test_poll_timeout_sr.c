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

/* Test case for poll timeout */
TEST_CASE_SELF(event_test_poll_timeout_sr)
{
    int i;

    /* Initialize the task */
    os_task_init(&eventq_task_poll_timeout_s, "eventq_task_poll_timeout_s",
        eventq_task_poll_timeout_send, NULL, SEND_TASK_POLL_TIMEOUT_PRIO,
        OS_WAIT_FOREVER, eventq_task_stack_poll_timeout_s, POLL_STACK_SIZE);

    /* Receive events and check whether the eevnts are correctly received */
    os_task_init(&eventq_task_poll_timeout_r, "eventq_task_timeout_r",
        eventq_task_poll_timeout_receive, NULL, RECEIVE_TASK_POLL_TIMEOUT_PRIO,
        OS_WAIT_FOREVER, eventq_task_stack_poll_timeout_r, POLL_STACK_SIZE);

    /* Initializing the eventqs. */
    for (i = 0; i < SIZE_MULTI_EVENT; i++){
        os_eventq_init(&multi_eventq[i]);

        m_event[i].ev_arg = NULL;
    }
}
