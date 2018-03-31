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
#ifndef _MBUF_TEST_H
#define _MBUF_TEST_H

#include <string.h>
#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "os_test_priv.h"

#ifdef __cplusplus
#extern "C" {
#endif

/* 
 * NOTE: currently, the buffer size cannot be changed as some tests are
 * hard-coded for this size.
 */
#define MBUF_TEST_POOL_BUF_SIZE     (256)
#define MBUF_TEST_POOL_BUF_COUNT    (10)

#define MBUF_TEST_DATA_LEN          (1024)

extern os_membuf_t os_mbuf_membuf[OS_MEMPOOL_SIZE(MBUF_TEST_POOL_BUF_SIZE,
        MBUF_TEST_POOL_BUF_COUNT)];

extern struct os_mbuf_pool os_mbuf_pool;
extern struct os_mempool os_mbuf_mempool;
extern uint8_t os_mbuf_test_data[MBUF_TEST_DATA_LEN];

void os_mbuf_test_setup(void);
void os_mbuf_test_misc_assert_sane(struct os_mbuf *om, void *data, int buflen,
                                   int pktlen, int pkthdr_len);

#ifdef __cplusplus
}
#endif

#endif /* _MBUF_TEST_H */
