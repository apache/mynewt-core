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
#ifndef _EVENTQ_TEST_H
#define _EVENTQ_TEST_H
 
#include <string.h>
#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "os_test_priv.h"

#ifdef __cplusplus
#extern "C" {
#endif

#if MYNEWT_VAL(SELFTEST)
#define MY_STACK_SIZE                   (5120)
#define POLL_STACK_SIZE                 (4096)
#else
#define MY_STACK_SIZE                   (128)
#define POLL_STACK_SIZE                 (32) /* for now */
#endif

#define INITIAL_EVENTQ_TASK_PRIO        (10)

/* Task 1 sending task */
/* Define task stack and task object */
#define SEND_TASK_PRIO                  (INITIAL_EVENTQ_TASK_PRIO + 1)
extern struct os_task eventq_task_s;
extern os_stack_t eventq_task_stack_s[MY_STACK_SIZE];

/* Task 2 receiving task */
#define RECEIVE_TASK_PRIO               (INITIAL_EVENTQ_TASK_PRIO + 2)
extern struct os_task eventq_task_r;
extern os_stack_t eventq_task_stack_r[MY_STACK_SIZE];

extern struct os_eventq my_eventq;

#define SIZE_MULTI_EVENT       (4)
extern struct os_eventq multi_eventq[SIZE_MULTI_EVENT];

/* This is to set the events we will use below */
extern struct os_event g_event;
extern struct os_event m_event[SIZE_MULTI_EVENT];

/* Setting the event to send and receive multiple data */
extern uint8_t my_event_type;

/* Setting up data for the poll */
/* Define the task stack for the eventq_task_poll_send */
#define SEND_TASK_POLL_PRIO             (INITIAL_EVENTQ_TASK_PRIO + 3)
extern struct os_task eventq_task_poll_s;
extern os_stack_t eventq_task_stack_poll_s[POLL_STACK_SIZE];

/* Define the task stack for the eventq_task_poll_receive */
#define RECEIVE_TASK_POLL_PRIO          (INITIAL_EVENTQ_TASK_PRIO + 4)
extern struct os_task eventq_task_poll_r;
extern os_stack_t eventq_task_stack_poll_r[POLL_STACK_SIZE ];

/* Setting the data for the poll timeout */
/* Define the task stack for the eventq_task_poll_timeout_send */
#define SEND_TASK_POLL_TIMEOUT_PRIO     (INITIAL_EVENTQ_TASK_PRIO + 5)
extern struct os_task eventq_task_poll_timeout_s;
extern os_stack_t eventq_task_stack_poll_timeout_s[POLL_STACK_SIZE];

/* Define the task stack for the eventq_task_poll_receive */
#define RECEIVE_TASK_POLL_TIMEOUT_PRIO  (INITIAL_EVENTQ_TASK_PRIO + 6)
extern struct os_task eventq_task_poll_timeout_r;
extern os_stack_t eventq_task_stack_poll_timeout_r[POLL_STACK_SIZE];

/* Setting the data for the poll single */
/* Define the task stack for the eventq_task_poll_single_send */
#define SEND_TASK_POLL_SINGLE_PRIO      (INITIAL_EVENTQ_TASK_PRIO + 7)
extern struct os_task eventq_task_poll_single_s;
extern os_stack_t eventq_task_stack_poll_single_s[POLL_STACK_SIZE];

/* Define the task stack for the eventq_task_poll_single_receive */
#define RECEIVE_TASK_POLL_SINGLE_PRIO   (INITIAL_EVENTQ_TASK_PRIO + 8)
extern struct os_task eventq_task_poll_single_r;
extern os_stack_t eventq_task_stack_poll_single_r[POLL_STACK_SIZE];

void eventq_task_send(void *arg);
void eventq_task_receive(void *arg);
void eventq_task_poll_send(void *arg);
void eventq_task_poll_receive(void *arg);
void eventq_task_poll_timeout_send(void *arg);
void eventq_task_poll_timeout_receive(void *arg);
void eventq_task_poll_single_send(void *arg);
void eventq_task_poll_single_receive(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _EVENTQ_TEST_H */
