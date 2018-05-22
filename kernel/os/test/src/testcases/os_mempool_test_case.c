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
 * os mempool test
 *
 * Main test loop for memory pool testing.
 *
 * @return int
 */
void
mempool_test(int num_blocks, int block_size, bool clear)
{
    int cnt;
    int true_block_size;
    int mem_pool_size;
    uint8_t *tstptr;
    void **free_ptr;
    void *block;
    os_error_t rc;

    /* Check for too many blocks */
    TEST_ASSERT(num_blocks <= MEMPOOL_TEST_MAX_BLOCKS);

    rc = os_mempool_init(&g_TstMempool, num_blocks, MEM_BLOCK_SIZE,
                         &TstMembuf[0], "TestMemPool");
    TEST_ASSERT_FATAL(rc == 0, "Error creating memory pool %d", rc);

retest:
    TEST_ASSERT(g_TstMempool.mp_num_free == num_blocks,
                "Number of free blocks not equal to total blocks!");

    TEST_ASSERT(SLIST_FIRST(&g_TstMempool) == (void *)&TstMembuf[0],
                "Free list pointer does not point to first block!");

    mem_pool_size = mempool_test_get_pool_size(num_blocks, block_size);
    TEST_ASSERT(mem_pool_size == TstMembufSz,
                "Total memory pool size not correct! (%d vs %lu)",
                mem_pool_size, (unsigned long)TstMembufSz);

    /* Get the real block size */
#if (OS_CFG_ALIGNMENT == OS_CFG_ALIGN_4)
    true_block_size = (g_TstMempool.mp_block_size + 3) & ~3;
#else
    true_block_size = (g_TstMempool.mp_block_size + 7) & ~7;
#endif

    /* Traverse free list. Better add up to number of blocks! */
    cnt = 0;
    free_ptr = (void **)TstMembuf;
    tstptr = (uint8_t *)TstMembuf;
    while (1) {
        /* Increment # of elements by 1 */
        ++cnt;

        /* If the free list is NULL, leave */
        if (*free_ptr == NULL) {
            break;
        }

        TEST_ASSERT(((uint8_t *)*free_ptr - (uint8_t *)free_ptr) ==
                        true_block_size,
                    "Free pointers are more than one block apart!");

        /* Move to next memory block */
        tstptr += true_block_size;

        TEST_ASSERT(*free_ptr == (void *)tstptr,
                    "Error: free_ptr=%p testptr=%p\n", *free_ptr, tstptr);

        free_ptr = *free_ptr;
    }

    /* Last one in list better be NULL */
    TEST_ASSERT(cnt == g_TstMempool.mp_num_blocks,
                "Free list contains too many elements (%u/%u)",
                cnt, g_TstMempool.mp_num_blocks);

    /* Get a block */
    block = os_memblock_get(&g_TstMempool);
    TEST_ASSERT(block != NULL,
                "Error: get block fails when pool should have elements");

    TEST_ASSERT(g_TstMempool.mp_num_free == (num_blocks-1),
                "Number of free blocks incorrect (%u vs %u)",
                g_TstMempool.mp_num_free, (num_blocks-1));

    /* Put back the block */
    rc = os_memblock_put(&g_TstMempool, block);
    TEST_ASSERT(rc == 0, "Put block fails with error code=%d\n", rc);

    TEST_ASSERT(g_TstMempool.mp_num_free == num_blocks,
                "Number of free blocks incorrect (%u vs %u)",
                g_TstMempool.mp_num_free, num_blocks);

    /* remove all the blocks. Make sure we get count. */
    memset(block_array, 0, sizeof(block_array));
    cnt = 0;
    while (1) {
        block = os_memblock_get(&g_TstMempool);
        if (block == NULL) {
            break;
        }
        block_array[cnt] = block;
        ++cnt;
        if (cnt == MEMPOOL_TEST_MAX_BLOCKS) {
            break;
        }
    }

    TEST_ASSERT((cnt == g_TstMempool.mp_num_blocks) &&
                (cnt != MEMPOOL_TEST_MAX_BLOCKS),
                "Got more blocks than mempool contains (%d vs %d)",
                cnt, g_TstMempool.mp_num_blocks);

    /* Better be no free blocks left! */
    TEST_ASSERT(g_TstMempool.mp_num_free == 0,
                "Got all blocks but number free not zero! (%d)",
                g_TstMempool.mp_num_free);

    /* clear mempool and rerun tests */
    if (clear) {
        clear = false;
        rc = os_mempool_clear(&g_TstMempool);
        TEST_ASSERT_FATAL(rc == 0, "Error reseting memory pool %d", rc);

        goto retest;
    }

    /* Now put them all back */
    for (cnt = 0; cnt < g_TstMempool.mp_num_blocks; ++cnt) {
        rc = os_memblock_put(&g_TstMempool, block_array[cnt]);
        TEST_ASSERT(rc == 0,
                    "Error putting back block %p (cnt=%d err=%d)",
                    block_array[cnt], cnt, rc);
    }

    /* Better be no free blocks left! */
    TEST_ASSERT(g_TstMempool.mp_num_free == g_TstMempool.mp_num_blocks,
                "Put all blocks but number free not equal to total!");

    /* Better get error when we try these things! */
    rc = os_memblock_put(NULL, block_array[0]);
    TEST_ASSERT(rc != 0,
                "Should have got an error trying to put to null pool");

    rc = os_memblock_put(&g_TstMempool, NULL);
    TEST_ASSERT(rc != 0, "No error trying to put to NULL block");

    TEST_ASSERT(os_memblock_get(NULL) == NULL,
                "No error trying to get a block from NULL pool");

}

TEST_CASE(os_mempool_test_case)
{
    mempool_test(NUM_MEM_BLOCKS, MEM_BLOCK_SIZE, false);
    mempool_test(NUM_MEM_BLOCKS, MEM_BLOCK_SIZE, true);
}
