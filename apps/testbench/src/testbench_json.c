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
#include <stdio.h>
#include <string.h>
#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "testbench.h"

#ifndef JSON_BIGBUF_SIZE
#define JSON_BIGBUF_SIZE 192
#endif

extern char *bigbuf;

void
testbench_json_init(void *arg)
{
    /*
     * Lorem ipsum dolor sit amet, consectetur adipiscing elit,
     * sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.
     */
    LOG_DEBUG(&testlog, LOG_MODULE_TEST,
             "%s testbench json_init", buildID);

    bigbuf = os_malloc(JSON_BIGBUF_SIZE);

    tu_suite_set_pass_cb(testbench_ts_pass, NULL);
    tu_suite_set_fail_cb(testbench_ts_fail, NULL);
}

void
testbench_json_complete(void *arg)
{
    os_free((void*)bigbuf);
}

TEST_CASE_DECL(test_json_simple_encode);
TEST_CASE_DECL(test_json_simple_decode);

TEST_SUITE(testbench_json_suite)
{
    LOG_DEBUG(&testlog, LOG_MODULE_TEST, "%s testbench_json", buildID);

    tu_suite_set_init_cb(testbench_json_init, NULL);
    tu_suite_set_complete_cb(testbench_json_complete, NULL);

    test_json_simple_encode();
    test_json_simple_decode();
}

int
testbench_json()
{
    tu_suite_set_init_cb(testbench_json_init, NULL);
    tu_suite_set_complete_cb(testbench_json_complete, NULL);
    LOG_DEBUG(&testlog, LOG_MODULE_TEST, "%s testbench_json", buildID);
    testbench_json_suite();

    return tu_any_failed;
}
