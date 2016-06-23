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

#define MY_STACK_SIZE        (5120)

/*Task 1 sending task*/
/* Define task stack and task object */
#define SEND_TASK_PRIO        (1)
struct os_task eventq_task_s;
os_stack_t eventq_task_stack_s[MY_STACK_SIZE];

/*Task 2 receiving task*/
#define RECEIVE_TASK_PRIO     (2)
struct os_task eventq_task_r;
os_stack_t eventq_task_stack_r[MY_STACK_SIZE];

struct os_eventq my_eventq;

#define SIZE_MULTI_EVENT        (4)
struct os_eventq multi_eventq[SIZE_MULTI_EVENT];

/* This is to set the events we will use below */
struct os_event g_event;
struct os_event m_event[SIZE_MULTI_EVENT];

/* Setting the event to send and receive multiple data */
uint8_t my_event_type = 1;

/* This is the task function  to send data */
void
eventq_task_send(void *arg)
{
    int i;

    g_event.ev_queued = 0;
    g_event.ev_type = my_event_type;
    g_event.ev_arg = NULL;

    os_eventq_put(&my_eventq, &g_event);

    os_time_delay(OS_TICKS_PER_SEC / 2);

    for (i = 0; i < SIZE_MULTI_EVENT; i++){
        m_event[i].ev_type = i + 2;
        m_event[i].ev_arg = NULL;

        /* Put and send */
        os_eventq_put(&multi_eventq[i], &m_event[i]);
        os_time_delay(OS_TICKS_PER_SEC / 2);
    }

    /* This task sleeps until the receive task completes the test. */
    os_time_delay(1000000);
}

/* This is the task function is the receiving function */
void
eventq_task_receive(void *arg)
{
    struct os_event *event;
    int i;

    event = os_eventq_get(&my_eventq);
    TEST_ASSERT(event->ev_type == my_event_type);

    /* Receiving multi event from the send task */
    for (i = 0; i < SIZE_MULTI_EVENT; i++) {
        event = os_eventq_get(&multi_eventq[i]);
        TEST_ASSERT(event->ev_type == i + 2);
    }

    /* Finishes the test when OS has been started */
    os_test_restart();
}

TEST_CASE(event_test_sr)
{
    int i;

    /* Initializing the OS */
    os_init();
    /* Initialize the task */
    os_task_init(&eventq_task_s, "eventq_task_s", eventq_task_send, NULL,
        SEND_TASK_PRIO, OS_WAIT_FOREVER, eventq_task_stack_s, MY_STACK_SIZE);

    /* Receive events and check whether the eevnts are correctly received */
    os_task_init(&eventq_task_r, "eventq_task_r", eventq_task_receive, NULL,
        RECEIVE_TASK_PRIO, OS_WAIT_FOREVER, eventq_task_stack_r,
        MY_STACK_SIZE);

    os_eventq_init(&my_eventq);

    for (i = 0; i < SIZE_MULTI_EVENT; i++){
        os_eventq_init(&multi_eventq[i]);
    }

    /* Does not return until OS_restart is called */
    os_start();

}

TEST_SUITE(os_eventq_test_suite)
{
    event_test_sr();
}
