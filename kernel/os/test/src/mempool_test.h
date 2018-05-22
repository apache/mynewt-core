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
#ifndef _MEMPOOL_TEST_H
#define _MEMPOOL_TEST_H

#include <stdio.h>
#include <string.h>
#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "os_test_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Limit max blocks for testing */
#ifndef MEMPOOL_TEST_MAX_BLOCKS
#define MEMPOOL_TEST_MAX_BLOCKS     (128)
#endif

/* Create a memory pool for testing */
#ifndef MEM_BLOCK_SIZE
#define MEM_BLOCK_SIZE  (80)
#endif

#ifndef NUM_MEM_BLOCKS
#define NUM_MEM_BLOCKS  (10)
#endif

extern int alignment;

/* Test memory pool structure */
extern struct os_mempool g_TstMempool;

/* Test memory pool buffer */
extern os_membuf_t *TstMembuf;
extern uint32_t TstMembufSz;

/* Array of block pointers. */
extern void *block_array[MEMPOOL_TEST_MAX_BLOCKS];

extern int verbose;

int mempool_test_get_pool_size(int num_blocks, int block_size);

void mempool_test(int num_blocks, int block_size, bool clear);

#ifdef __cplusplus
}
#endif

#endif /* _MEMPOOL_TEST_H */
