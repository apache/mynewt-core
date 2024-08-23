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
#ifndef _MSYS_TEST_H
#define _MSYS_TEST_H

#include <string.h>
#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "os_test_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

STAILQ_HEAD(os_mbuf_list, os_mbuf_pool);
struct os_mbuf_list *get_msys_pool_list(void);
STAILQ_HEAD(os_mempool_list, os_mempool);

struct msys_context {
    struct os_mbuf_list mbuf_list;
    struct os_mempool_list mempool_list;
};

#define MSYS_TEST_POOL_BIG_BUF_SIZE     (444)
#define MSYS_TEST_POOL_BIG_BUF_COUNT    (10)
#define MSYS_TEST_POOL_MED_BUF_SIZE     (255)
#define MSYS_TEST_POOL_MED_BUF_COUNT    (10)
#define MSYS_TEST_POOL_SMALL_BUF_SIZE   (67)
#define MSYS_TEST_POOL_SMALL_BUF_COUNT  (10)

#define MSYS_TEST_SMALL_BUF_SIZE        (MSYS_TEST_POOL_SMALL_BUF_SIZE - sizeof(struct os_mbuf))
#define MSYS_TEST_MED_BUF_SIZE          (MSYS_TEST_POOL_MED_BUF_SIZE - sizeof(struct os_mbuf))
#define MSYS_TEST_BIG_BUF_SIZE          (MSYS_TEST_POOL_BIG_BUF_SIZE - sizeof(struct os_mbuf))

extern os_membuf_t msys_mbuf_membuf1[];
extern os_membuf_t msys_mbuf_membuf2[];
extern os_membuf_t msys_mbuf_membuf3[];

extern struct os_mbuf_pool os_mbuf_pool;
extern struct os_mempool os_mbuf_mempool;
extern uint8_t os_mbuf_test_data[MBUF_TEST_DATA_LEN];

void os_msys_test_setup(int pool_count, struct msys_context *context);
void os_msys_test_teardown(struct msys_context *context);
void os_msys_test_misc_assert_sane(struct os_mbuf *om, void *data, int buflen,
                                   int pktlen, int pkthdr_len);

#ifdef __cplusplus
}
#endif

#endif /* _MSYS_TEST_H */
