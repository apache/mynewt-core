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

/* Limit max blocks for testing */
#define MEMPOOL_TEST_MAX_BLOCKS     (128)

/* Create a memory pool for testing */
#ifndef MEM_BLOCK_SIZE
#define MEM_BLOCK_SIZE  (80)
#endif

#ifndef MEM_MEM_BLOCKS
#define NUM_MEM_BLOCKS  (10)
#endif

/* Test memory pool structure */
struct os_mempool g_TstMempool;

/* Test memory pool buffer */
os_membuf_t *TstMembuf;
extern uint32_t TstMembufSz;

/* Array of block pointers. */
void *block_array[MEMPOOL_TEST_MAX_BLOCKS];

void
testbench_mempool_init(void *arg)
{
    /*
     * Lorem ipsum dolor sit amet, consectetur adipiscing elit,
     * sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.
     */
    LOG_DEBUG(&testlog, LOG_MODULE_TEST,
             "%s testbench mempool_init", buildID);

    TstMembufSz = (sizeof(os_membuf_t) *
                   OS_MEMPOOL_SIZE(NUM_MEM_BLOCKS, MEM_BLOCK_SIZE));
    TstMembuf = os_malloc(TstMembufSz);

    tu_suite_set_pass_cb(testbench_ts_pass, NULL);
    tu_suite_set_fail_cb(testbench_ts_fail, NULL);
}

void
testbench_mempool_complete(void *arg)
{
    os_free((void*)TstMembuf);
}

TEST_CASE_DECL(os_mempool_test_case)

TEST_SUITE(testbench_mempool_suite)
{
    LOG_DEBUG(&testlog, LOG_MODULE_TEST, "%s testbench_mempool", buildID);

    tu_suite_set_init_cb(testbench_mempool_init, NULL);
    tu_suite_set_complete_cb(testbench_mempool_complete, NULL);

    os_mempool_test_case();
}

int
testbench_mempool()
{
    tu_suite_set_init_cb(testbench_mempool_init, NULL);
    tu_suite_set_complete_cb(testbench_mempool_complete, NULL);
    LOG_DEBUG(&testlog, LOG_MODULE_TEST, "%s testbench_mempool", buildID);
    testbench_mempool_suite();

    return tu_any_failed;
}
