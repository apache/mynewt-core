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

#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "os_test_priv.h"

/* Task 1 for sending */
struct os_task callout_task_struct_send;
os_stack_t callout_task_stack_send[CALLOUT_STACK_SIZE];

struct os_task callout_task_struct_receive;
os_stack_t callout_task_stack_receive[CALLOUT_STACK_SIZE];

/* Declaring variables for callout */
struct os_callout callout_test_c;

/* The event to be sent*/
struct os_eventq callout_evq;
struct os_event callout_ev;

/* The callout_stop task */
struct os_task callout_task_struct_stop_send;
os_stack_t callout_task_stack_stop_send[CALLOUT_STACK_SIZE];

struct os_task callout_task_struct_stop_receive;
os_stack_t callout_task_stack_stop_receive[CALLOUT_STACK_SIZE];

/* Delearing variables for callout_stop_func */
struct os_callout callout_stop_test[MULTI_SIZE];

/* The event to be sent*/
struct os_eventq callout_stop_evq[MULTI_SIZE];
struct os_event callout_stop_ev;

/* Declearing varables for callout_speak */
struct os_task callout_task_struct_speak;
os_stack_t callout_task_stack_speak[CALLOUT_STACK_SIZE];

/* Declearing varaibles for listen */
struct os_task callout_task_struct_listen;
os_stack_t callout_task_stack_listen[CALLOUT_STACK_SIZE];

struct os_callout callout_speak;

/* Global variables to be used by the callout functions */
int p;
int q;
int t;

/* This is the function for callout_init*/
void
my_callout(struct os_event *ev)
{
    p = 4;
}

/* This is the function for callout_init of stop test_case*/
void
my_callout_stop_func(struct os_event *ev)
{
    q = 1;
}
/* This is the function for callout_init for speak test_case*/
void
my_callout_speak_func(struct os_event *ev)
{
    t = 2;
}

/* This is a callout task to send data */
void
callout_task_send(void *arg)
{
   int i;
    /* Should say whether callout is armed or not */
    i= os_callout_queued(&callout_test_c);
    TEST_ASSERT(i == 0);

    /* Arm the callout */
    i = os_callout_reset(&callout_test_c, OS_TICKS_PER_SEC/ 50);
    TEST_ASSERT_FATAL(i == 0);

    /* Should say whether callout is armed or not */
    i = os_callout_queued(&callout_test_c);
    TEST_ASSERT(i == 1);

    /* Send the callout */ 
    os_time_delay(OS_TICKS_PER_SEC );
}

/* This is the callout to receive data */
void
callout_task_receive(void *arg)
{
    int i;
    struct os_event *event;
    struct os_callout *callout;
    os_time_t now;
    os_time_t tm;
    os_sr_t sr; 

    /* Recieve using the os_eventq_poll */
    event = os_eventq_poll(&callout_test_c.c_evq, 1, OS_WAIT_FOREVER);
    TEST_ASSERT(event->ev_arg == NULL);
    callout = (struct os_callout *)event;
    TEST_ASSERT(callout->c_ev.ev_cb == my_callout);

    /* Should say whether callout is armed or not */
    i = os_callout_queued(&callout_test_c);
    TEST_ASSERT(i == 0);

    OS_ENTER_CRITICAL(sr);
    now = os_time_get();
    tm = os_callout_wakeup_ticks(now);
    TEST_ASSERT(tm == OS_TIMEOUT_NEVER);
    OS_EXIT_CRITICAL(sr);

    /* Finishes the test when OS has been started */
    os_test_restart();
}

/* This is callout to send the stop_callout */
void
callout_task_stop_send(void *arg)
{
    int k;
    int j;
     /* Should say whether callout is armed or not */
    for(k = 0; k<MULTI_SIZE; k++){
        j = os_callout_queued(&callout_stop_test[k]);
        TEST_ASSERT(j == 0);
    }

    /* Show that  callout is not armed after calling callout_stop */
    for(k = 0; k<MULTI_SIZE; k++){
        os_callout_stop(&callout_stop_test[k]);
        j = os_callout_queued(&callout_stop_test[k]);
        TEST_ASSERT(j == 0);
    }
    /* Arm the callout */
    for(k = 0; k<MULTI_SIZE; k++){
        j = os_callout_reset(&callout_stop_test[k], OS_TICKS_PER_SEC/ 50);
        TEST_ASSERT_FATAL(j == 0);
    }
    os_time_delay( OS_TICKS_PER_SEC );
}

/* This is the callout to receive stop_callout data */
void
callout_task_stop_receive(void *arg)
{
    int k;
    struct os_event *event;
    struct os_callout *callout;
    /* Recieving using the os_eventq_poll */
    for(k=0; k<MULTI_SIZE; k++){
        event = os_eventq_poll(&callout_stop_test[k].c_evq, 1,
           OS_WAIT_FOREVER);
        TEST_ASSERT(event->ev_arg == NULL);
        callout = (struct os_callout *)event;
        TEST_ASSERT(callout->c_ev.ev_cb == my_callout_stop_func);


     }

    /* Show that event is removed from the queued after calling callout_stop */
    for(k=0; k<MULTI_SIZE; k++){
        os_callout_stop(&callout_stop_test[k]);
        /* Testing that the event has been removed from queue */
        TEST_ASSERT_FATAL(1);
     }
    /* Finishes the test when OS has been started */
    os_test_restart();

}

/* This is a callout task to send data */
void
callout_task_stop_speak(void *arg)
{
    int i;
    /* Arm the callout */
    i = os_callout_reset(&callout_speak, OS_TICKS_PER_SEC/ 50);
    TEST_ASSERT_FATAL(i == 0);

    /* should say whether callout is armed or not */
    i = os_callout_queued(&callout_speak);
    TEST_ASSERT(i == 1);

    os_callout_stop(&callout_speak);

    /* Send the callout */ 
    os_time_delay(OS_TICKS_PER_SEC/ 100 );
    /* Finishes the test when OS has been started */
    os_test_restart();
}

void
callout_task_stop_listen(void *arg)
{
    struct os_event *event;
    struct os_callout *callout;

    event = os_eventq_get(callout_speak.c_evq);
    TEST_ASSERT_FATAL(0);
    callout = (struct os_callout *)event;
    TEST_ASSERT(callout->c_ev.ev_cb == my_callout_speak_func);

}

TEST_CASE_DECL(callout_test_speak)
TEST_CASE_DECL(callout_test_stop)
TEST_CASE_DECL(callout_test)

TEST_SUITE(os_callout_test_suite)
{
    callout_test();
    callout_test_stop();
    callout_test_speak();
}
