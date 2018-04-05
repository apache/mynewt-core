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
#ifndef _CALLOUT_TEST_H
#define _CALLOUT_TEST_H

#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "os_test_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INITIAL_CALLOUT_TASK_PRIO       (20)

/* Task 1 for sending */
#define CALLOUT_STACK_SIZE              (5120)
#define SEND_CALLOUT_TASK_PRIO          (INITIAL_CALLOUT_TASK_PRIO + 0)
extern struct os_task callout_task_struct_send;
extern os_stack_t callout_task_stack_send[CALLOUT_STACK_SIZE];

#define RECEIVE_CALLOUT_TASK_PRIO       (INITIAL_CALLOUT_TASK_PRIO + 1)
extern struct os_task callout_task_struct_receive;
extern os_stack_t callout_task_stack_receive[CALLOUT_STACK_SIZE];

/* The event to be sent*/
extern struct os_eventq callout_evq;
extern struct os_event callout_ev;

/* The callout_stop task */
#define SEND_STOP_CALLOUT_TASK_PRIO     (INITIAL_CALLOUT_TASK_PRIO + 2)
extern struct os_task callout_task_struct_stop_send;
extern os_stack_t callout_task_stack_stop_send[CALLOUT_STACK_SIZE];

#define RECEIVE_STOP_CALLOUT_TASK_PRIO  (INITIAL_CALLOUT_TASK_PRIO + 3)
extern struct os_task callout_task_struct_stop_receive;
extern os_stack_t callout_task_stack_stop_receive[CALLOUT_STACK_SIZE];

/* Delearing variables for callout_stop_func */
#define MULTI_SIZE    (2)
extern struct os_callout callout_stop_test[MULTI_SIZE];

/* The event to be sent*/
extern struct os_eventq callout_stop_evq[MULTI_SIZE];
extern struct os_event callout_stop_ev;

/* Declearing varables for callout_speak */
#define SPEAK_CALLOUT_TASK_PRIO         (INITIAL_CALLOUT_TASK_PRIO + 4)
extern struct os_task callout_task_struct_speak;
extern os_stack_t callout_task_stack_speak[CALLOUT_STACK_SIZE];

/* Declearing varaibles for listen */
#define LISTEN_CALLOUT_TASK_PRIO        (INITIAL_CALLOUT_TASK_PRIO + 5)
extern struct os_task callout_task_struct_listen;
extern os_stack_t callout_task_stack_listen[CALLOUT_STACK_SIZE];

extern struct os_callout callout_speak;
extern struct os_callout callout_test_c;

/* Global variables to be used by the callout functions */
extern int p;
extern int q;
extern int t;

void my_callout(struct os_event *ev);
void my_callout_stop_func(struct os_event *ev);
void my_callout_speak_func(struct os_event *ev);
void callout_task_send(void *arg);
void callout_task_receive(void *arg);
void callout_task_stop_send(void *arg);
void callout_task_stop_receive(void *arg);
void callout_task_stop_speak(void *arg);
void callout_task_stop_listen(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _CALLOUT_TEST_H */
