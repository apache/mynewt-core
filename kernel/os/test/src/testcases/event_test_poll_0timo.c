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

/**
 * Tests eventq_poll() with a timeout of 0.  This should not involve the
 * scheduler at all, so it should work without starting the OS.
 */
TEST_CASE_SELF(event_test_poll_0timo)
{
    struct os_eventq *eventqs[SIZE_MULTI_EVENT];
    struct os_event *evp;
    struct os_event ev;
    int i;

    for (i = 0; i < SIZE_MULTI_EVENT; i++){
        os_eventq_init(&multi_eventq[i]);
        eventqs[i] = &multi_eventq[i];
    }

    evp = os_eventq_poll(eventqs, SIZE_MULTI_EVENT, 0);
    TEST_ASSERT(evp == NULL);

    /* Ensure no eventq thinks a task is waiting on it. */
    for (i = 0; i < SIZE_MULTI_EVENT; i++) {
        TEST_ASSERT(eventqs[i]->evq_task == NULL);
    }

    /* Put an event on one of the queues. */
    memset(&ev, 0, sizeof ev);
    os_eventq_put(eventqs[3], &ev);

    evp = os_eventq_poll(eventqs, SIZE_MULTI_EVENT, 0);
    TEST_ASSERT(evp == &ev);
}
