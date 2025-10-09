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

#define MBUF_CNT            3
#define SMALL_PAYLOAD_BYTES 96
#define LARGE_PAYLOAD_BYTES 256

/* Outer guard region to detect writes off the arena ends */
#define OUTER_GUARD_PAD  32
#define OUTER_GUARD_BYTE 0xA5

/* Inner guard region to detect writes past an element's payload */
#define INNER_GUARD_BYTES 16
#define INNER_GUARD_BYTE  0xC3

/* Optional user header in front of payload (we don't use it here) */
#define USER_HDR_SZ 0

/* One memblock contains: mbuf header + pkthdr + user hdr + aligned payload */
#define MBUF_BLOCK_OVERHEAD                                                   \
    (sizeof(struct os_mbuf) + sizeof(struct os_mbuf_pkthdr) + (USER_HDR_SZ))

/* Size of a single mempool element for a given payload size */
#define MBUF_BLOCK_SIZE(payload_len)                                          \
    (MBUF_BLOCK_OVERHEAD + OS_ALIGN((payload_len), OS_ALIGNMENT))

/* Total arena bytes = outer guard + blocks + outer guard */
#define ARENA_BYTES(payload_len)                                              \
    (OUTER_GUARD_PAD +                                                        \
     OS_MEMPOOL_BYTES(MBUF_CNT, MBUF_BLOCK_SIZE(payload_len)) + OUTER_GUARD_PAD)

static uint8_t small_arena[ARENA_BYTES(SMALL_PAYLOAD_BYTES)];
static uint8_t large_arena[ARENA_BYTES(LARGE_PAYLOAD_BYTES)];

static struct os_mempool small_mp, large_mp;
static struct os_mbuf_pool small_omp, large_omp;

/* Base pointer to the inner (usable) part of each arena */
static uint8_t *small_base;
static uint8_t *large_base;

static void
outer_guard_paint(uint8_t *arena, size_t total_bytes)
{
    memset(arena, OUTER_GUARD_BYTE, OUTER_GUARD_PAD);
    memset(arena + total_bytes - OUTER_GUARD_PAD, OUTER_GUARD_BYTE, OUTER_GUARD_PAD);
}

static void
outer_guard_check(const uint8_t *arena, size_t total_bytes)
{
    uint8_t i;

    for (i = 0; i < OUTER_GUARD_PAD; i++) {
        TEST_ASSERT(arena[i] == OUTER_GUARD_BYTE, "Outer guard (left) corrupted");
        TEST_ASSERT(arena[total_bytes - OUTER_GUARD_PAD + i] == OUTER_GUARD_BYTE,
                    "Outer guard (right) corrupted");
    }
}

/**
 * Guard helpers (inner, per element)
 * We mark IN-MEMBLOCK bytes *just after the payload* so any memcpy
 * that overruns a destination immediately flips these sentinels.
 */

static void
inner_guards_paint_pool(uint8_t *base, size_t block_size, uint8_t elem_count,
                        size_t payload_len)
{
    const size_t payload_aligned = OS_ALIGN(payload_len, OS_ALIGNMENT);
    const size_t guard_off = MBUF_BLOCK_OVERHEAD + payload_aligned;
    uint8_t i;

    if (guard_off + INNER_GUARD_BYTES > block_size) {
        return;
    }

    for (i = 0; i < elem_count; i++) {
        uint8_t *elt = base + (size_t)i * block_size;
        memset(elt + guard_off, INNER_GUARD_BYTE, INNER_GUARD_BYTES);
    }
}

static void
inner_guards_check_pool(const uint8_t *base, size_t block_size, uint8_t elem_count,
                        size_t payload_len, const char *pool_name)
{
    const size_t payload_aligned = OS_ALIGN(payload_len, OS_ALIGNMENT);
    const size_t guard_off = MBUF_BLOCK_OVERHEAD + payload_aligned;
    const uint8_t *elt;
    uint8_t i, j;

    if (guard_off + INNER_GUARD_BYTES > block_size) {
        return;
    }

    for (i = 0; i < elem_count; i++) {
        elt = base + (size_t)i * block_size;
        for (j = 0; j < INNER_GUARD_BYTES; j++) {
            TEST_ASSERT(elt[guard_off + j] == INNER_GUARD_BYTE,
                        "Inner guard corrupted in %s (elt=%d, +%d)", pool_name,
                        i, j);
        }
    }
}

static void
init_two_pools(void)
{
    int rc;

    outer_guard_paint(small_arena, sizeof(small_arena));
    outer_guard_paint(large_arena, sizeof(large_arena));

    small_base = small_arena + OUTER_GUARD_PAD;
    large_base = large_arena + OUTER_GUARD_PAD;

    rc = os_mempool_init(&small_mp, MBUF_CNT, MBUF_BLOCK_SIZE(SMALL_PAYLOAD_BYTES),
                         small_base, "small_mp");
    TEST_ASSERT_FATAL(rc == 0);

    rc = os_mempool_init(&large_mp, MBUF_CNT, MBUF_BLOCK_SIZE(LARGE_PAYLOAD_BYTES),
                         large_base, "large_mp");
    TEST_ASSERT_FATAL(rc == 0);

    rc = os_mbuf_pool_init(&small_omp, &small_mp,
                           MBUF_BLOCK_SIZE(SMALL_PAYLOAD_BYTES), MBUF_CNT);
    TEST_ASSERT_FATAL(rc == 0);

    rc = os_mbuf_pool_init(&large_omp, &large_mp,
                           MBUF_BLOCK_SIZE(LARGE_PAYLOAD_BYTES), MBUF_CNT);
    TEST_ASSERT_FATAL(rc == 0);

    inner_guards_paint_pool(small_base, MBUF_BLOCK_SIZE(SMALL_PAYLOAD_BYTES),
                            MBUF_CNT, SMALL_PAYLOAD_BYTES);
    inner_guards_paint_pool(large_base, MBUF_BLOCK_SIZE(LARGE_PAYLOAD_BYTES),
                            MBUF_CNT, LARGE_PAYLOAD_BYTES);
}

/**
 * Scenario:
 *   - Head from SMALL pool
 *   - Tail from LARGE pool
 *   - Duplicate mbuf chain
 *
 * Verify:
 *   1) Second duplicate segment was allocated from the SAME pool
 *      as the original second segment (catches wrong allocator logic).
 *   2) Segment lengths and data match.
 *   3) Both OUTER and INNER guards remained intact (catches overruns).
 */
TEST_CASE_SELF(os_mbuf_test_dup_pool)
{
    struct os_mbuf *head_small = NULL;
    struct os_mbuf *tail_large = NULL;
    struct os_mbuf *dup = NULL;
    struct os_mbuf *orig2;
    struct os_mbuf *dup2;
    size_t i;

    /* Leave a bit of headroom on the small buffer to avoid fuzzy edge cases */
    uint8_t data0[SMALL_PAYLOAD_BYTES - 8];
    uint8_t data1[200];

    /* Deterministic fill patterns */
    for (i = 0; i < sizeof(data0); i++) {
        data0[i] = (uint8_t)(i ^ 0x11);
    }

    for (i = 0; i < sizeof(data1); i++) {
        data1[i] = (uint8_t)(i ^ 0x6B);
    }

    init_two_pools();

    /* Build original chain: small -> large */
    head_small = os_mbuf_get(&small_omp, 0);
    TEST_ASSERT_FATAL(head_small != NULL);
    TEST_ASSERT_FATAL(os_mbuf_append(head_small, data0, sizeof(data0)) == 0);
    TEST_ASSERT(head_small->om_len == sizeof(data0), "Small head om_len mismatch");

    tail_large = os_mbuf_get(&large_omp, 0);
    TEST_ASSERT_FATAL(tail_large != NULL);
    TEST_ASSERT_FATAL(os_mbuf_append(tail_large, data1, sizeof(data1)) == 0);
    TEST_ASSERT(tail_large->om_len == sizeof(data1), "Large tail om_len mismatch");

    os_mbuf_concat(head_small, tail_large);

    /* Duplicate */
    dup = os_mbuf_dup(head_small);
    TEST_ASSERT_FATAL(dup != NULL);
    TEST_ASSERT_FATAL(dup != head_small);
    TEST_ASSERT_FATAL(SLIST_NEXT(dup, om_next) != NULL);

    /* 1) Pool-identity check – catches wrong allocator logic even w/o overflow */
    orig2 = SLIST_NEXT(head_small, om_next);
    dup2 = SLIST_NEXT(dup, om_next);
    TEST_ASSERT_FATAL(orig2 && dup2);
    TEST_ASSERT(dup2->om_omp == orig2->om_omp,
                "Second dup segment allocated from wrong pool");

    /* 2) Length & data checks */
    TEST_ASSERT(dup->om_len == sizeof(data0), "Dup head length mismatch");
    TEST_ASSERT(dup2->om_len == sizeof(data1), "Dup second length mismatch");

    TEST_ASSERT(memcmp(OS_MBUF_DATA(dup, uint8_t *), data0, sizeof(data0)) == 0,
                "Dup head payload differs");
    TEST_ASSERT(memcmp(OS_MBUF_DATA(dup2, uint8_t *), data1, sizeof(data1)) == 0,
                "Dup second payload differs");

    /* 3) Robust guard checks (outer + inner, both pools) */
    outer_guard_check(small_arena, sizeof(small_arena));
    outer_guard_check(large_arena, sizeof(large_arena));

    inner_guards_check_pool(small_base, MBUF_BLOCK_SIZE(SMALL_PAYLOAD_BYTES),
                            MBUF_CNT, SMALL_PAYLOAD_BYTES, "small");
    inner_guards_check_pool(large_base, MBUF_BLOCK_SIZE(LARGE_PAYLOAD_BYTES),
                            MBUF_CNT, LARGE_PAYLOAD_BYTES, "large");

    /* Cleanup */
    TEST_ASSERT_FATAL(os_mbuf_free_chain(head_small) == 0);
    TEST_ASSERT_FATAL(os_mbuf_free_chain(dup) == 0);
}
