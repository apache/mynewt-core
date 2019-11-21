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

#if MBUF_TEST_POOL_BUF_SIZE != 256
    #error "Test pool buffer size must be 256!"
#endif

/* This structure is used to create mbuf chains. It contains the length
   of data that should be in each mbuf in the chain and the amount of
   leading space in the mbuf */
struct os_mbtpc_cd
{
    uint16_t mlen;
    uint16_t leadingspace;
};

/* The random seed (chosen at random) */
static unsigned long int rand_next = 1001;

/* Used for data integrity testing */
static uint8_t os_mbuf_test_pack_chains_test_data[2048];

/**
 * Calculates the total number of mbufs needed for a chain
 * presuming each mbuf is filled to capacity except the last.
 * NOTE: the pkthdr_len must include the os_mbuf_pkthdr struct;
 * it is not automatically accounted for.
 */
uint16_t
os_mbuf_test_pack_chains_calc_total_mbufs(uint16_t len, int pkthdr_len)
{
    uint16_t total;
    uint16_t rem;
    uint16_t dbuflen;

    rem = len;
    dbuflen = os_mbuf_pool.omp_databuf_len;

    /* Only the first mbuf should have a packet header so this is subtracted
       for only one mbuf (if present) */
    total = 1;
    if (len > (dbuflen - pkthdr_len)) {
        rem -= (dbuflen - pkthdr_len);
        while (rem > 0) {
            ++total;
            if (rem <= dbuflen) {
                break;
            }
            rem -= dbuflen;
        }
    }

    return total;
}

struct os_mbuf *
os_mbuf_test_pack_chains_create_chain(int num_mbufs, struct os_mbtpc_cd *mdata,
                                      uint16_t srcoff, int is_pkthdr,
                                      uint8_t pkthdr_len)
{
    int i;
    int rc;
    struct os_mbuf *m;
    struct os_mbuf *cur;
    struct os_mbuf *tmp;
    uint8_t *src;
    uint16_t hdrlen;

    TEST_ASSERT_FATAL(mdata != NULL, "mdata NULL");
    TEST_ASSERT_FATAL(num_mbufs != 0, "mbufs cannot be zero");

    if (is_pkthdr) {
        m = os_mbuf_get_pkthdr(&os_mbuf_pool, pkthdr_len);
        m->om_data += mdata[0].leadingspace;
        hdrlen = sizeof(struct os_mbuf_pkthdr) + pkthdr_len;
    } else {
        m = os_mbuf_get(&os_mbuf_pool, mdata[0].leadingspace);
        hdrlen = 0;
    }
    os_mbuf_test_misc_assert_sane(m, NULL, 0, 0, hdrlen);
    TEST_ASSERT_FATAL(mdata[0].mlen != 0, "mlen cannot be zero");

    src = os_mbuf_test_pack_chains_test_data + srcoff;
    rc = os_mbuf_copyinto(m, 0, src, (int)mdata[0].mlen);
    TEST_ASSERT_FATAL(rc == 0, "copyinto failed");
    src += mdata[0].mlen;

    cur = m;
    for (i = 1; i < num_mbufs; ++i) {
        tmp = os_mbuf_get(&os_mbuf_pool, mdata[i].leadingspace);
        os_mbuf_test_misc_assert_sane(tmp, NULL, 0, 0, 0);
        rc = os_mbuf_copyinto(tmp, 0, src, (int)mdata[i].mlen);
        TEST_ASSERT_FATAL(rc == 0, "copyinto failed");
        if (is_pkthdr) {
            OS_MBUF_PKTLEN(m) += mdata[i].mlen;
        }
        src += mdata[i].mlen;
        SLIST_NEXT(cur, om_next) = tmp;
        cur = tmp;
    }
    return m;
}

/*
 * This is here cause I dont feel like calling rand :-)
 *
 *      Taken from the K&R C programming language book. Page 46.
 *      returns a pseudo-random integer of 0..32767. Note that
 *      this is compatible with the System V function rand(), not
 *      with the bsd function rand() that returns 0..(2**31)-1.
 */
unsigned int
os_mbuf_test_pack_chains_rand(void)
{
	rand_next = rand_next * 1103515245 + 12345;
	return ((unsigned int)(rand_next / 65536) % 32768);
}

/*
 * Traverses an mbuf chain and tests to make sure that all mbufs
 * are fully utilized. The last mbuf in the chain may not be full
 * but all others must be. No mbuf should have zero length. This
 * also tests that the data pointer in the mbuf is in the correct
 * location (points to start of data).
 */
static void
os_mbuf_test_pack_chains_ensure_full(struct os_mbuf *om)
{
    struct os_mbuf *m;
    struct os_mbuf *next;
    uint8_t *dptr;
    uint16_t startoff;
    uint16_t dlen;

    m = om;

    while (m) {
        TEST_ASSERT_FATAL(m->om_len != 0, "om_len cannot be zero");
        next = SLIST_NEXT(m, om_next);
        if (OS_MBUF_IS_PKTHDR(m)) {
            startoff = m->om_pkthdr_len;
        } else {
            startoff = 0;
        }
        dptr = (uint8_t *)&m->om_databuf[0] + startoff;
        TEST_ASSERT_FATAL(m->om_data == dptr,"om_data incorrect");

        TEST_ASSERT_FATAL(m->om_omp->omp_databuf_len > startoff,
                          "pool databuf len incorrect");
        dlen = m->om_omp->omp_databuf_len - startoff;
        if (next) {
            TEST_ASSERT_FATAL(m->om_len == dlen, "mbuf not full");
        }
        m = next;
    }
}

TEST_CASE_SELF(os_mbuf_test_pack_chains)
{
    uint8_t *src;
    uint16_t num_free_start;
    uint16_t totlen;
    uint16_t n;
    struct os_mbuf *m1;
    struct os_mbuf *m2;
    struct os_mbtpc_cd mcd[8];
    int rc;

    src = &os_mbuf_test_pack_chains_test_data[0];
    os_mbuf_test_setup();

    /* Fill the test data with random data */
    for (rc = 0; rc < 2048; ++rc) {
        os_mbuf_test_pack_chains_test_data[rc] =
            (uint8_t)os_mbuf_test_pack_chains_rand();
    }

    /*
     * TEST 1: Single mbuf w/pkthdr. Test no change or corruption
     * This test has om_data at the start so nothing should be done
     */
    m1 = os_mbuf_get(&os_mbuf_pool, 0);
    os_mbuf_test_misc_assert_sane(m1, NULL, 0, 0, 0);

    rc = os_mbuf_copyinto(m1, 0, src, 50);
    TEST_ASSERT_FATAL(rc == 0, "copyinto failure");
    os_mbuf_pack_chains(m1, NULL);
    os_mbuf_test_pack_chains_ensure_full(m1);
    os_mbuf_test_misc_assert_sane(m1, src, 50, 50, 0);
    os_mbuf_free(m1);
    TEST_ASSERT_FATAL(os_mbuf_mempool.mp_num_free == MBUF_TEST_POOL_BUF_COUNT,
                      "mempool num free incorrect");

    /*
     * TEST 2: Single mbuf w/pkthdr. This has om_data moved so
     * pack should move the data to the start.
     */
    m1 = os_mbuf_get_pkthdr(&os_mbuf_pool, 16);
    os_mbuf_test_misc_assert_sane(m1, NULL, 0, 0,
                                  sizeof(struct os_mbuf_pkthdr) + 16);
    m1->om_data += 13;
    rc = os_mbuf_copyinto(m1, 0, src, 77);
    TEST_ASSERT_FATAL(rc == 0, "copyinto failure");
    os_mbuf_pack_chains(m1, NULL);
    os_mbuf_test_pack_chains_ensure_full(m1);
    os_mbuf_test_misc_assert_sane(m1, src, 77, 77,
                                  sizeof(struct os_mbuf_pkthdr) + 16);
    os_mbuf_free_chain(m1);
    TEST_ASSERT_FATAL(os_mbuf_mempool.mp_num_free == MBUF_TEST_POOL_BUF_COUNT,
                      "mempool num free incorrect");

    /*
     * TEST 3: Two chains. Make sure a single chain with full
     * buffers. Both m1 and m2 have packet headers.
     */
    num_free_start = os_mbuf_mempool.mp_num_free;
    mcd[0].leadingspace = 0;
    mcd[0].mlen = 99;
    mcd[1].leadingspace = 10;
    mcd[1].mlen = 43;
    mcd[2].leadingspace = 0;
    mcd[2].mlen = 67;
    m1 = os_mbuf_test_pack_chains_create_chain(3, &mcd[0], 0, 1, 0);
    TEST_ASSERT_FATAL(m1 != NULL, "alloc failure");
    mcd[0].leadingspace = 0;
    mcd[0].mlen = os_mbuf_pool.omp_databuf_len - sizeof(struct os_mbuf_pkthdr);
    mcd[1].leadingspace = 0;
    mcd[1].mlen = os_mbuf_pool.omp_databuf_len;
    m2 = os_mbuf_test_pack_chains_create_chain(2, &mcd[0], 99 + 43 + 67, 1, 0);
    TEST_ASSERT_FATAL(m2 != NULL, "alloc failure");
    m1 = os_mbuf_pack_chains(m1, m2);
    TEST_ASSERT_FATAL(m1 != NULL, "pack chain failure");
    os_mbuf_test_pack_chains_ensure_full(m1);
    totlen = 99 + 43 + 67 + mcd[0].mlen + mcd[1].mlen;

    /* NOTE: mcd[0].mlen contains the length of a maximum size first mbuf */
    os_mbuf_test_misc_assert_sane(m1, src, mcd[0].mlen, totlen,
                                  sizeof(struct os_mbuf_pkthdr));
    n = os_mbuf_test_pack_chains_calc_total_mbufs(totlen,
                                                  sizeof(struct os_mbuf_pkthdr));
    TEST_ASSERT_FATAL(os_mbuf_mempool.mp_num_free == (num_free_start - n),
                      "number free incorrect. mp_num_free=%u num_free=%u n=%u",
                      os_mbuf_mempool.mp_num_free, num_free_start, n);
    os_mbuf_free_chain(m1);
    TEST_ASSERT_FATAL(os_mbuf_mempool.mp_num_free == MBUF_TEST_POOL_BUF_COUNT,
                      "mpool has incorrect number of free buffers");

    /* TEST 4: a zero length mbuf in middle and at end */
    num_free_start = os_mbuf_mempool.mp_num_free;
    mcd[0].leadingspace = 0;
    mcd[0].mlen = 24;
    mcd[1].leadingspace = 50;
    mcd[1].mlen = 0;
    mcd[2].leadingspace = 0;
    mcd[2].mlen = 33;
    m1 = os_mbuf_test_pack_chains_create_chain(3, &mcd[0], 0, 1, 0);
    TEST_ASSERT_FATAL(m1 != NULL, "alloc failure");
    mcd[0].leadingspace = 0;
    mcd[0].mlen = 100;
    mcd[1].leadingspace = 0;
    mcd[1].mlen = 0;
    m2 = os_mbuf_test_pack_chains_create_chain(2, &mcd[0], 24 + 33, 0, 0);
    TEST_ASSERT_FATAL(m2 != NULL, "alloc failure");
    m1 = os_mbuf_pack_chains(m1, m2);
    TEST_ASSERT_FATAL(m1 != NULL, "pack chain failure");
    os_mbuf_test_pack_chains_ensure_full(m1);
    totlen = 24 + 33 + 100;

    /* NOTE: mcd[0].mlen contains the length of a maximum size first mbuf */
    os_mbuf_test_misc_assert_sane(m1, src, 157, totlen,
                                  sizeof(struct os_mbuf_pkthdr));
    n = os_mbuf_test_pack_chains_calc_total_mbufs(totlen,
                                                  sizeof(struct os_mbuf_pkthdr));
    TEST_ASSERT_FATAL(os_mbuf_mempool.mp_num_free == (num_free_start - n),
                      "number free incorrect. mp_num_free=%u num_free=%u n=%u",
                      os_mbuf_mempool.mp_num_free, num_free_start, n);
    os_mbuf_free_chain(m1);
    TEST_ASSERT_FATAL(os_mbuf_mempool.mp_num_free == MBUF_TEST_POOL_BUF_COUNT,
                      "mpool has incorrect number of free buffers");

    /* TEST 5: All full*/
    num_free_start = os_mbuf_mempool.mp_num_free;
    mcd[0].leadingspace = 0;
    mcd[0].mlen = os_mbuf_pool.omp_databuf_len - sizeof(struct os_mbuf_pkthdr);
    mcd[1].leadingspace = 0;
    mcd[1].mlen = os_mbuf_pool.omp_databuf_len;
    mcd[2].leadingspace = 0;
    mcd[2].mlen = os_mbuf_pool.omp_databuf_len;
    mcd[3].leadingspace = 0;
    mcd[3].mlen = os_mbuf_pool.omp_databuf_len;
    m1 = os_mbuf_test_pack_chains_create_chain(4, &mcd[0], 0, 1, 0);
    TEST_ASSERT_FATAL(m1 != NULL, "alloc failure");
    mcd[0].leadingspace = 0;
    mcd[0].mlen = os_mbuf_pool.omp_databuf_len;
    mcd[1].leadingspace = 0;
    mcd[1].mlen = os_mbuf_pool.omp_databuf_len;
    mcd[2].leadingspace = 0;
    mcd[2].mlen = os_mbuf_pool.omp_databuf_len;
    totlen = (4 * os_mbuf_pool.omp_databuf_len) - sizeof(struct os_mbuf_pkthdr);
    m2 = os_mbuf_test_pack_chains_create_chain(3, &mcd[0], totlen, 0, 0);
    TEST_ASSERT_FATAL(m2 != NULL, "alloc failure");
    m1 = os_mbuf_pack_chains(m1, m2);
    TEST_ASSERT_FATAL(m1 != NULL, "pack chain failure");
    os_mbuf_test_pack_chains_ensure_full(m1);
    totlen += mcd[0].mlen + mcd[1].mlen + mcd[2].mlen;

    /* NOTE: mcd[0].mlen contains the length of a maximum size first mbuf */
    mcd[0].mlen = os_mbuf_pool.omp_databuf_len - sizeof(struct os_mbuf_pkthdr);
    os_mbuf_test_misc_assert_sane(m1, src, mcd[0].mlen, totlen,
                                  sizeof(struct os_mbuf_pkthdr));
    n = os_mbuf_test_pack_chains_calc_total_mbufs(totlen,
                                                  sizeof(struct os_mbuf_pkthdr));
    TEST_ASSERT_FATAL(os_mbuf_mempool.mp_num_free == (num_free_start - n),
                      "number free incorrect. mp_num_free=%u num_free=%u n=%u",
                      os_mbuf_mempool.mp_num_free, num_free_start, n);
    os_mbuf_free_chain(m1);
    TEST_ASSERT_FATAL(os_mbuf_mempool.mp_num_free == MBUF_TEST_POOL_BUF_COUNT,
                      "mpool has incorrect number of free buffers");

    /* TEST 6: consecutive zero mbufs */
    num_free_start = os_mbuf_mempool.mp_num_free;
    mcd[0].leadingspace = 0;
    mcd[0].mlen = os_mbuf_pool.omp_databuf_len - sizeof(struct os_mbuf_pkthdr);
    mcd[1].leadingspace = 8;
    mcd[1].mlen = 0;
    mcd[2].leadingspace = 11;
    mcd[2].mlen = 0;
    mcd[3].leadingspace = 20;
    mcd[3].mlen = 44;
    m1 = os_mbuf_test_pack_chains_create_chain(4, &mcd[0], 0, 1, 0);
    TEST_ASSERT_FATAL(m1 != NULL, "alloc failure");
    mcd[0].leadingspace = 0;
    mcd[0].mlen = os_mbuf_pool.omp_databuf_len - sizeof(struct os_mbuf_pkthdr);
    totlen = (os_mbuf_pool.omp_databuf_len - sizeof(struct os_mbuf_pkthdr)) + 44;
    m2 = os_mbuf_test_pack_chains_create_chain(1, &mcd[0], totlen, 1, 0);
    TEST_ASSERT_FATAL(m2 != NULL, "alloc failure");
    m1 = os_mbuf_pack_chains(m1, m2);
    TEST_ASSERT_FATAL(m1 != NULL, "pack chain failure");
    os_mbuf_test_pack_chains_ensure_full(m1);
    totlen += mcd[0].mlen;

    /* NOTE: mcd[0].mlen contains the length of a maximum size first mbuf */
    mcd[0].mlen = os_mbuf_pool.omp_databuf_len - sizeof(struct os_mbuf_pkthdr);
    os_mbuf_test_misc_assert_sane(m1, src, mcd[0].mlen, totlen,
                                  sizeof(struct os_mbuf_pkthdr));
    n = os_mbuf_test_pack_chains_calc_total_mbufs(totlen,
                                                  sizeof(struct os_mbuf_pkthdr));
    TEST_ASSERT_FATAL(os_mbuf_mempool.mp_num_free == (num_free_start - n),
                      "number free incorrect. mp_num_free=%u num_free=%u n=%u",
                      os_mbuf_mempool.mp_num_free, num_free_start, n);
    os_mbuf_free_chain(m1);
    TEST_ASSERT_FATAL(os_mbuf_mempool.mp_num_free == MBUF_TEST_POOL_BUF_COUNT,
                      "mpool has incorrect number of free buffers");
}
