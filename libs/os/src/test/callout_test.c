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

#define RECEIVE_CALLOUT_TASK_PRIO        (2)
struct os_task callout_task_struct_receive;
os_stack_t callout_task_stack_receive[CALLOUT_STACK_SIZE];

/* Delearing variables for callout_func */
struct os_callout_func callout_func_test;

/* The event to be sent*/
struct os_eventq callout_evq;
struct os_event callout_ev;

 int i; 
 
/* This is the function for callout_init*/
void my_callout_func(void *arg)
{
    int p;
    p = 4;
    TEST_ASSERT(p==4);
}

/* This is a callout task to send data */
void
callout_task_send()
{
   
    /* should say whether callout is armed or not */
    i = os_callout_queued(&callout_func_test.cf_c);
    TEST_ASSERT(i == 0);

    /* Arm the callout */
    i = os_callout_reset(&callout_func_test.cf_c, OS_TICKS_PER_SEC/ 50);
    TEST_ASSERT_FATAL(i == 0);

    /* should say whether callout is armed or not */
    i = os_callout_queued(&callout_func_test.cf_c);
    TEST_ASSERT(i == 1);

    /* Send the callout */ 
    os_time_delay(OS_TICKS_PER_SEC );
}

void
callout_task_receive(void *arg)
{
    struct os_event *event;
    /* Recieving using the os_eventq_poll*/
        event = os_eventq_poll(&callout_func_test.cf_c.c_evq, 1, OS_WAIT_FOREVER);
        TEST_ASSERT(event->ev_type ==  OS_EVENT_T_TIMER);
        TEST_ASSERT(event->ev_arg == NULL);

    TEST_ASSERT(i == 1);
    /* should say whether callout is armed or not */
    i = os_callout_queued(&callout_func_test.cf_c);
    TEST_ASSERT(i == 0);

    /* Finishes the test when OS has been started */
    os_test_restart();

}

TEST_CASE(callout_test)
{

    /* Initializing the OS */
    os_init();
    
    /* Initialize the sending task */
    os_task_init(&callout_task_struct_send, "callout_task_send",
        callout_task_send, NULL, SEND_CALLOUT_TASK_PRIO, OS_WAIT_FOREVER,
        callout_task_stack_send, CALLOUT_STACK_SIZE);

    /* Initialize the sending task */
    os_task_init(&callout_task_struct_receive, "callout_task_receive",
        callout_task_receive, NULL, RECEIVE_CALLOUT_TASK_PRIO, OS_WAIT_FOREVER,
        callout_task_stack_receive, CALLOUT_STACK_SIZE);

    os_eventq_init(&callout_evq);
    
    /* Initialize the callout function */
    os_callout_func_init(&callout_func_test, &callout_evq,
        my_callout_func, NULL);

    /* Does not return until OS_restart is called */
    os_start();

}

TEST_SUITE(os_callout_test_suite)
{   
    callout_test();
}
