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

#include "rwlock/rwlock.h"
#include "rwlock_test.h"

#define RTCB_READ_TASK_PRIO     10
#define RTCB_WRITE_TASK_PRIO    11

#define RTCB_STACK_SIZE         1024

static void rtcb_evcb_read(struct os_event *ev);
static void rtcb_evcb_write(struct os_event *ev);

static int rtcb_num_readers;
static int rtcb_num_writers;

static struct os_eventq rtcb_evq_read;
static struct os_eventq rtcb_evq_write;

static struct os_task rtcb_task_read;
static struct os_task rtcb_task_write;

static os_stack_t rtcb_stack_read[RTCB_STACK_SIZE];
static os_stack_t rtcb_stack_write[RTCB_STACK_SIZE];

static struct rwlock rtcb_rwlock;

static struct os_event rtcb_ev_read = {
    .ev_cb = rtcb_evcb_read,
};

static struct os_event rtcb_ev_write = {
    .ev_cb = rtcb_evcb_write,
};

static void
rtcb_evcb_read(struct os_event *ev)
{
    rwlock_acquire_read(&rtcb_rwlock);
    rtcb_num_readers++;
}

static void
rtcb_evcb_write(struct os_event *ev)
{
    rwlock_acquire_write(&rtcb_rwlock);
    rtcb_num_writers++;
}

static void
rtcb_enqueue_read(void)
{
    os_eventq_put(&rtcb_evq_read, &rtcb_ev_read);
}

static void
rtcb_enqueue_write(void)
{
    os_eventq_put(&rtcb_evq_write, &rtcb_ev_write);
}

static void
rtcb_read_task_handler(void *arg)
{
    while (1) {
        os_eventq_run(&rtcb_evq_read);
    }
}

static void
rtcb_write_task_handler(void *arg)
{
    while (1) {
        os_eventq_run(&rtcb_evq_write);
    }
}

TEST_CASE_TASK(rwlock_test_case_basic)
{
    int rc;

    os_eventq_init(&rtcb_evq_read);
    os_eventq_init(&rtcb_evq_write);

    rc = os_task_init(&rtcb_task_read, "read", rtcb_read_task_handler, NULL,
                      RTCB_READ_TASK_PRIO, OS_WAIT_FOREVER, rtcb_stack_read,
                      RTCB_STACK_SIZE);
    TEST_ASSERT_FATAL(rc == 0);

    rc = os_task_init(&rtcb_task_write, "write", rtcb_write_task_handler, NULL,
                      RTCB_WRITE_TASK_PRIO, OS_WAIT_FOREVER, rtcb_stack_write,
                      RTCB_STACK_SIZE);
    TEST_ASSERT_FATAL(rc == 0);

    rwlock_init(&rtcb_rwlock);

    /* Enqueue one read; ensure it acquires the lock. */
    rtcb_enqueue_read();
    TEST_ASSERT_FATAL(rtcb_num_readers == 1);
    TEST_ASSERT_FATAL(rtcb_num_writers == 0);

    /* Enqueue one write; ensure it does *not* acquire the lock. */
    rtcb_enqueue_write();
    TEST_ASSERT_FATAL(rtcb_num_readers == 1);
    TEST_ASSERT_FATAL(rtcb_num_writers == 0);

    /* Enqueue two more readers; ensure both do *not* acquire the lock. */
    rtcb_enqueue_read();
    rtcb_enqueue_read();
    TEST_ASSERT_FATAL(rtcb_num_readers == 1);
    TEST_ASSERT_FATAL(rtcb_num_writers == 0);

    /* Release reader; ensure lock given to writer. */
    rwlock_release_read(&rtcb_rwlock);
    TEST_ASSERT_FATAL(rtcb_num_readers == 1);
    TEST_ASSERT_FATAL(rtcb_num_writers == 1);

    /* Release writer; ensure lock given to both pending readers. */
    rwlock_release_write(&rtcb_rwlock);
    TEST_ASSERT_FATAL(rtcb_num_readers == 3);
    TEST_ASSERT_FATAL(rtcb_num_writers == 1);

    /*** Reset state. */
    rwlock_init(&rtcb_rwlock);
    rtcb_num_readers = 0;
    rtcb_num_writers = 0;

    /* Enqueue two writers; ensure lock given to one writer. */
    rtcb_enqueue_write();
    rtcb_enqueue_write();
    TEST_ASSERT_FATAL(rtcb_num_readers == 0);
    TEST_ASSERT_FATAL(rtcb_num_writers == 1);

    /* Enqueue two readers; ensure lock not given. */
    rtcb_enqueue_read();
    rtcb_enqueue_read();
    TEST_ASSERT_FATAL(rtcb_num_readers == 0);
    TEST_ASSERT_FATAL(rtcb_num_writers == 1);

    /* Release writer; ensure lock given to second writer. */
    rwlock_release_write(&rtcb_rwlock);
    TEST_ASSERT_FATAL(rtcb_num_readers == 0);
    TEST_ASSERT_FATAL(rtcb_num_writers == 2);

    /* Release writer; ensure lock given to both readers. */
    rwlock_release_write(&rtcb_rwlock);
    TEST_ASSERT_FATAL(rtcb_num_readers == 2);
    TEST_ASSERT_FATAL(rtcb_num_writers == 2);
}
