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
#ifndef __RUNTEST_H__
#define __RUNTEST_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Adds the run test commands to your shell/newtmgr.
 */
void runtest_init(void);

/*
 * Retrieves the event queue used by the runtest package.
 */
struct os_eventq *run_evq_get(void);

/*
 * Callback for runtest events - newtmgr uses this to add
 * run test requests to default queue for test application (e.g., mynewtsanity)
 */
void run_evcb_set(os_event_fn *cb);

/*
 * Token is append to log messages - for use by ci gateway
 */
#define RUNTEST_REQ_SIZE  32
extern char runtest_test_token[RUNTEST_REQ_SIZE];

/*
 * Argument struct passed in from "run" requests via newtmgr
 */
struct runtest_evq_arg {
    char* run_testname;
    char* run_token;
};

#ifdef __cplusplus
}
#endif

#endif /* __RUNTEST_H__ */
