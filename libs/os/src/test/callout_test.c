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
 
#include "testutil/testutil.h"
#include "os/os.h"
#include "os_test_priv.h"
#include "os/os_eventq.h"
#include "os/os_callout.h"

/* Task 1 for sending */
#define CALLOUT_STACK_SIZE        (5120)
#define SEND_CALLOUT_TASK_PRIO        (1)
struct os_task callout_task_struct_send;
os_stack_t callout_task_stack_send[CALLOUT_STACK_SIZE];

/* Initializing the callout 
struct os_event callout_event;
struct os_eventq callout_eventq;
struct os_eventq callout_eventq_delivered;
struct os_callout struct_callout;
*/

/* Delearing variables for callout_func */
struct os_callout_func callout_func;
struct os_eventq callout_eventq;
void native_cputimer_cb(void *arg);

/* This is a callout task to send data */
void
callout_task_send()
{
    int i;
    /*
    struct_callout.c_ev = callout_event;
    struct_callout.c_evq = &callout_eventq;
    struct_callout.c_ticks = OS_TICKS_PER_SEC / 20;
    */
    /* Initialize the callout
    os_callout_init(&struct_callout, &callout_eventq_delivered, NULL);
    */

    /* Initialize the callout function */
    os_callout_func_init(&callout_func, &callout_eventq,
        native_cputimer_cb, NULL);
    i = os_callout_queued((struct os_callout )callout_func);
    TEST_ASSERT(i == 0);
}

TEST_CASE(callout_test){

    /* Initializing the OS */
    os_init();
    
    /* Initialize the sending task */
    os_task_init(&callout_task_struct_send, "callout_task_send",
        callout_task_send, NULL, SEND_CALLOUT_TASK_PRIO, OS_WAIT_FOREVER,
        callout_task_stack_send, CALLOUT_STACK_SIZE);

    /* Does not return until OS_restart is called */
    os_start();

}

TEST_SUITE(os_callout_test_suite){   
    callout_test();
}
