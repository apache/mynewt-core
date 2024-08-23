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
#include "msys_test.h"

TEST_CASE_SELF(os_msys_test_limit1) {
    const size_t buf_count = MSYS_TEST_POOL_BIG_BUF_COUNT;
    struct os_mbuf *m[buf_count + 1];
    int i;
    struct msys_context context;

    os_msys_test_setup(1, &context);

    TEST_ASSERT_FATAL(os_msys_num_free() == buf_count,
                      "mempool wrong number of buffers %d <> %d",
                      (int)os_msys_num_free(), buf_count);

    for (i = 0; i < buf_count; ++i) {
        m[i] = os_msys_get(0, 0);
        TEST_ASSERT_FATAL(m[i] != NULL, "os_msys_get() failed to allocate mbuf");
        TEST_ASSERT_FATAL(os_msys_num_free() == buf_count - i - 1,
                          "mempool wrong number of buffers %d <> %d",
                          (int)os_msys_num_free(), buf_count);
    }
    TEST_ASSERT_FATAL(os_msys_num_free() == 0, "msys should be empty");

    /* Next alloc should fail */
    m[i] = os_msys_get(0, 0);
    TEST_ASSERT_FATAL(m[buf_count] == NULL, "msys should be empty");

    for (i = 0; i < buf_count; ++i) {
        os_mbuf_free(m[i]);
        TEST_ASSERT_FATAL(os_msys_num_free() == i + 1,
                          "mempool wrong number of buffers %d <> %d",
                          (int)os_msys_num_free(), i + 1);
    }
    os_msys_test_teardown(&context);
}

TEST_CASE_SELF(os_msys_test_limit2) {
    const size_t buf_count = MSYS_TEST_POOL_BIG_BUF_COUNT + MSYS_TEST_POOL_SMALL_BUF_COUNT;
    struct os_mbuf *m[buf_count + 1];
    int i;
    struct msys_context context;

    os_msys_test_setup(2, &context);

    TEST_ASSERT_FATAL(os_msys_num_free() == buf_count,
                      "mempool wrong number of buffers %d <> %d",
                      (int)os_msys_num_free(), buf_count);

    for (i = 0; i < buf_count; ++i) {
        m[i] = os_msys_get(0, 0);
        TEST_ASSERT_FATAL(m[i] != NULL, "os_msys_get() failed to allocate mbuf");
        TEST_ASSERT_FATAL(os_msys_num_free() == buf_count - i - 1,
                          "mempool wrong number of buffers %d <> %d",
                          (int)os_msys_num_free(), buf_count);
    }
    TEST_ASSERT_FATAL(os_msys_num_free() == 0, "msys should be empty");

    /* Next alloc should fail */
    m[i] = os_msys_get(0, 0);
    TEST_ASSERT_FATAL(m[buf_count] == NULL, "msys should be empty");

    for (i = 0; i < buf_count; ++i) {
        os_mbuf_free(m[i]);
        TEST_ASSERT_FATAL(os_msys_num_free() == i + 1,
                          "mempool wrong number of buffers %d <> %d",
                          (int)os_msys_num_free(), i + 1);
    }
    os_msys_test_teardown(&context);
}

TEST_CASE_SELF(os_msys_test_limit3) {
    const size_t buf_count = MSYS_TEST_POOL_BIG_BUF_COUNT + MSYS_TEST_POOL_SMALL_BUF_COUNT +
                             MSYS_TEST_POOL_MED_BUF_COUNT;
    struct os_mbuf *m[buf_count + 1];
    int i;
    struct msys_context context;

    os_msys_test_setup(3, &context);

    TEST_ASSERT_FATAL(os_msys_num_free() == buf_count,
                      "mempool wrong number of buffers %d <> %d",
                      (int)os_msys_num_free(), buf_count);

    for (i = 0; i < buf_count; ++i) {
        m[i] = os_msys_get(0, 0);
        TEST_ASSERT_FATAL(m[i] != NULL, "os_msys_get() failed to allocate mbuf");
        TEST_ASSERT_FATAL(os_msys_num_free() == buf_count - i - 1,
                          "mempool wrong number of buffers %d <> %d",
                          (int)os_msys_num_free(), buf_count);
    }
    TEST_ASSERT_FATAL(os_msys_num_free() == 0, "msys should be empty");

    /* Next alloc should fail */
    m[i] = os_msys_get(0, 0);
    TEST_ASSERT_FATAL(m[buf_count] == NULL, "msys should be empty");

    for (i = 0; i < buf_count; ++i) {
        os_mbuf_free(m[i]);
        TEST_ASSERT_FATAL(os_msys_num_free() == i + 1,
                          "mempool wrong number of buffers %d <> %d",
                          (int)os_msys_num_free(), i + 1);
    }
    os_msys_test_teardown(&context);
}

TEST_CASE_SELF(os_msys_test_alloc1) {
    const size_t buf_count = MSYS_TEST_POOL_BIG_BUF_COUNT + MSYS_TEST_POOL_SMALL_BUF_COUNT +
                             MSYS_TEST_POOL_MED_BUF_COUNT;
    struct os_mbuf *m[buf_count + 1];
    struct os_mbuf *mb;
    int i;
    struct msys_context context;

    os_msys_test_setup(3, &context);

    TEST_ASSERT_FATAL(os_msys_num_free() == buf_count,
                      "mempool wrong number of buffers %d <> %d",
                      (int)os_msys_num_free(), buf_count);
    /* Small buffer should be taken from small buffer pool at this point */
    m[0] = mb = os_msys_get(MSYS_TEST_SMALL_BUF_SIZE / 2, 0);
    TEST_ASSERT_FATAL(mb != NULL, "os_msys_get() failed to allocate mbuf");
    TEST_ASSERT_FATAL(mb->om_omp->omp_databuf_len == MSYS_TEST_SMALL_BUF_SIZE,
                      "os_msys_get() allocated from wrong pool %s %d",
                      mb->om_omp->omp_pool->name, mb->om_omp->omp_databuf_len);

    /* Corner case, exact small size should be taken from small buffer pool */
    m[1] = mb = os_msys_get(MSYS_TEST_SMALL_BUF_SIZE, 0);
    TEST_ASSERT_FATAL(mb != NULL, "os_msys_get() failed to allocate mbuf");
    TEST_ASSERT_FATAL(mb->om_omp->omp_databuf_len == MSYS_TEST_SMALL_BUF_SIZE,
                      "os_msys_get() allocated from wrong pool %s %d",
                      mb->om_omp->omp_pool->name, mb->om_omp->omp_databuf_len);

    /* Now medium pool should be used */
    m[2] = mb = os_msys_get(MSYS_TEST_SMALL_BUF_SIZE + 1, 0);
    TEST_ASSERT_FATAL(mb != NULL, "os_msys_get() failed to allocate mbuf");
    TEST_ASSERT_FATAL(mb->om_omp->omp_databuf_len == MSYS_TEST_MED_BUF_SIZE,
                      "os_msys_get() allocated from wrong pool %s %d",
                      mb->om_omp->omp_pool->name, mb->om_omp->omp_databuf_len);

    /* Corner case for medium buffer */
    m[3] = mb = os_msys_get(MSYS_TEST_MED_BUF_SIZE, 0);
    TEST_ASSERT_FATAL(mb != NULL, "os_msys_get() failed to allocate mbuf");
    TEST_ASSERT_FATAL(mb->om_omp->omp_databuf_len == MSYS_TEST_MED_BUF_SIZE,
                      "os_msys_get() allocated from wrong pool %s %d",
                      mb->om_omp->omp_pool->name, mb->om_omp->omp_databuf_len);

    /* Corner case for big buffer */
    m[4] = mb = os_msys_get(MSYS_TEST_MED_BUF_SIZE + 1, 0);
    TEST_ASSERT_FATAL(mb != NULL, "os_msys_get() failed to allocate mbuf");
    TEST_ASSERT_FATAL(mb->om_omp->omp_databuf_len == MSYS_TEST_BIG_BUF_SIZE,
                      "os_msys_get() allocated from wrong pool %s %d",
                      mb->om_omp->omp_pool->name, mb->om_omp->omp_databuf_len);

    /* Bigger allocation should take from big buffer */
    m[5] = mb = os_msys_get(MSYS_TEST_BIG_BUF_SIZE + 1, 0);
    TEST_ASSERT_FATAL(mb != NULL, "os_msys_get() failed to allocate mbuf");
    TEST_ASSERT_FATAL(mb->om_omp->omp_databuf_len == MSYS_TEST_BIG_BUF_SIZE,
                      "os_msys_get() allocated from wrong pool %s %d",
                      mb->om_omp->omp_pool->name, mb->om_omp->omp_databuf_len);

    TEST_ASSERT_FATAL(os_msys_num_free() == buf_count - 6,
                      "mempool wrong number of buffers %d <> %d",
                      (int)os_msys_num_free(), buf_count - 6);

    /* Allocate all buffers as small ones */
    for (i = 6; i < buf_count; ++i) {
        m[i] = os_msys_get(MSYS_TEST_SMALL_BUF_SIZE / 2, 0);
        TEST_ASSERT_FATAL(m[i] != NULL, "os_msys_get() failed to allocate mbuf");
        TEST_ASSERT_FATAL(os_msys_num_free() == buf_count - i - 1,
                          "mempool wrong number of buffers %d <> %d",
                          (int)os_msys_num_free(), buf_count);
    }
    TEST_ASSERT_FATAL(os_msys_num_free() == 0, "msys should be empty");

    /* Next alloc should fail */
    m[i] = os_msys_get(0, 0);
    TEST_ASSERT_FATAL(m[buf_count] == NULL, "msys should be empty");

    for (i = 0; i < buf_count; ++i) {
        os_mbuf_free(m[i]);
        TEST_ASSERT_FATAL(os_msys_num_free() == i + 1,
                          "mempool wrong number of buffers %d <> %d",
                          (int)os_msys_num_free(), i + 1);
    }

    /* Allocate all buffers as big ones */
    for (i = 0; i < buf_count; ++i) {
        m[i] = os_msys_get(MSYS_TEST_BIG_BUF_SIZE + 10, 0);
        TEST_ASSERT_FATAL(m[i] != NULL, "os_msys_get() failed to allocate mbuf");
        TEST_ASSERT_FATAL(os_msys_num_free() == buf_count - i - 1,
                          "mempool wrong number of buffers %d <> %d",
                          (int)os_msys_num_free(), buf_count);
    }
    TEST_ASSERT_FATAL(os_msys_num_free() == 0, "msys should be empty");

    /* Next alloc should fail */
    m[i] = os_msys_get(0, 0);
    TEST_ASSERT_FATAL(m[buf_count] == NULL, "msys should be empty");

    for (i = 0; i < buf_count; ++i) {
        os_mbuf_free(m[i]);
        TEST_ASSERT_FATAL(os_msys_num_free() == i + 1,
                          "mempool wrong number of buffers %d <> %d",
                          (int)os_msys_num_free(), i + 1);
    }

    os_msys_test_teardown(&context);
}
