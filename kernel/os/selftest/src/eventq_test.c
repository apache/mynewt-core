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
 
#include <string.h>
#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "os_test_priv.h"

/* Task 1 sending task */
/* Define task stack and task object */
struct os_task eventq_task_s;
os_stack_t eventq_task_stack_s[MY_STACK_SIZE];

/* Task 2 receiving task */
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

/* Setting up data for the poll */
/* Define the task stack for the eventq_task_poll_send */
struct os_task eventq_task_poll_s;
os_stack_t eventq_task_stack_poll_s[POLL_STACK_SIZE];

/* Define the task stack for the eventq_task_poll_receive */
struct os_task eventq_task_poll_r;
os_stack_t eventq_task_stack_poll_r[POLL_STACK_SIZE ];

/* Setting the data for the poll timeout */
/* Define the task stack for the eventq_task_poll_timeout_send */
struct os_task eventq_task_poll_timeout_s;
os_stack_t eventq_task_stack_poll_timeout_s[POLL_STACK_SIZE];

/* Define the task stack for the eventq_task_poll_receive */
struct os_task eventq_task_poll_timeout_r;
os_stack_t eventq_task_stack_poll_timeout_r[POLL_STACK_SIZE];

/* Setting the data for the poll single */
/* Define the task stack for the eventq_task_poll_single_send */
struct os_task eventq_task_poll_single_s;
os_stack_t eventq_task_stack_poll_single_s[POLL_STACK_SIZE];

/* Define the task stack for the eventq_task_poll_single_receive */
struct os_task eventq_task_poll_single_r;
os_stack_t eventq_task_stack_poll_single_r[POLL_STACK_SIZE];

TEST_CASE_DECL(event_test_sr)
TEST_CASE_DECL(event_test_poll_sr)
TEST_CASE_DECL(event_test_poll_timeout_sr)
TEST_CASE_DECL(event_test_poll_single_sr)
TEST_CASE_DECL(event_test_poll_0timo)

/* This is the task function  to send data */
void
eventq_task_send(void *arg)
{
    int i;

    g_event.ev_queued = 0;
    g_event.ev_arg = (void *)(intptr_t)my_event_type;

    os_eventq_put(&my_eventq, &g_event);

    os_time_delay(OS_TICKS_PER_SEC / 2);

    for (i = 0; i < SIZE_MULTI_EVENT; i++){
        m_event[i].ev_arg = (void *)(intptr_t)(i + 2);

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
    TEST_ASSERT((intptr_t)event->ev_arg == my_event_type);

    /* Receiving multi event from the send task */
    for (i = 0; i < SIZE_MULTI_EVENT; i++) {
        event = os_eventq_get(&multi_eventq[i]);
        TEST_ASSERT((intptr_t)event->ev_arg == i + 2);
    }

    /* Finishes the test when OS has been started */
    os_test_restart();
}

void
eventq_task_poll_send(void *arg)
{
    struct os_eventq *eventqs[SIZE_MULTI_EVENT];
    int i;

    for (i = 0; i < SIZE_MULTI_EVENT; i++){
        eventqs[i] = &multi_eventq[i];
    }

    for (i = 0; i < SIZE_MULTI_EVENT; i++){
        m_event[i].ev_arg = (void *)(intptr_t)(i + 10);

        /* Put and send */
        os_eventq_put(eventqs[i], &m_event[i]);
        os_time_delay(OS_TICKS_PER_SEC / 2);
    }

    /* This task sleeps until the receive task completes the test. */
    os_time_delay(1000000);
}

void
eventq_task_poll_receive(void *arg)
{
    struct os_eventq *eventqs[SIZE_MULTI_EVENT];
    struct os_event *event;
    int i;

    for (i = 0; i < SIZE_MULTI_EVENT; i++){
        eventqs[i] = &multi_eventq[i];
    }

    /* Recieving using the os_eventq_poll*/
    for (i = 0; i < SIZE_MULTI_EVENT; i++) {
        event = os_eventq_poll(eventqs, SIZE_MULTI_EVENT, OS_WAIT_FOREVER);
        TEST_ASSERT((intptr_t)event->ev_arg == i +10);
    }

    /* Finishes the test when OS has been started */
    os_test_restart();

}

/* Sending with a time failure */
void
eventq_task_poll_timeout_send(void *arg)
{
    struct os_eventq *eventqs[SIZE_MULTI_EVENT];
    int i;

    for (i = 0; i < SIZE_MULTI_EVENT; i++){
        eventqs[i] = &multi_eventq[i];
    }

    for (i = 0; i < SIZE_MULTI_EVENT; i++){
         os_time_delay(OS_TICKS_PER_SEC);

        /* Put and send */
        os_eventq_put(eventqs[i], &m_event[i]);
        os_time_delay(OS_TICKS_PER_SEC / 2);
    }

    /* This task sleeps until the receive task completes the test. */
    os_time_delay(1000000);
}

/* Receiving multiple event queues with a time failure */
void
eventq_task_poll_timeout_receive(void *arg)
{
    struct os_eventq *eventqs[SIZE_MULTI_EVENT];
    struct os_event *event;
    int i;

    for (i = 0; i < SIZE_MULTI_EVENT; i++){
        eventqs[i] = &multi_eventq[i];
    }

    /* Recieving using the os_eventq_poll_timeout*/
    for (i = 0; i < SIZE_MULTI_EVENT; i++) {
        event = os_eventq_poll(eventqs, SIZE_MULTI_EVENT, OS_TICKS_PER_SEC / 5);
        TEST_ASSERT(event == NULL);
    }

    /* Finishes the test when OS has been started */
    os_test_restart();

}

/* Sending a single event to poll */
void
eventq_task_poll_single_send(void *arg)
{
    struct os_eventq *eventqs[SIZE_MULTI_EVENT];
    int i;
    int position = 2;

    for (i = 0; i < SIZE_MULTI_EVENT; i++){
        eventqs[i] = &multi_eventq[i];
    }

    /* Put and send */
    os_eventq_put(eventqs[position], &m_event[position]);
    os_time_delay(OS_TICKS_PER_SEC / 2);

    /* This task sleeps until the receive task completes the test. */
    os_time_delay(1000000);
}

/* Recieving the single event */
void
eventq_task_poll_single_receive(void *arg)
{
    struct os_eventq *eventqs[SIZE_MULTI_EVENT];
    struct os_event *event;
    int i;

    for (i = 0; i < SIZE_MULTI_EVENT; i++){
        eventqs[i] = &multi_eventq[i];
    }

    /* Recieving using the os_eventq_poll*/
    event = os_eventq_poll(eventqs, SIZE_MULTI_EVENT, OS_WAIT_FOREVER);
    TEST_ASSERT((intptr_t)event->ev_arg == 20);

    /* Finishes the test when OS has been started */
    os_test_restart();
}

TEST_SUITE(os_eventq_test_suite)
{
    event_test_sr();
    event_test_poll_sr();
    event_test_poll_timeout_sr();
    event_test_poll_single_sr();
    event_test_poll_0timo();
}
