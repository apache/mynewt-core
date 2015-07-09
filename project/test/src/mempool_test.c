/**
 * Copyright (c) 2015 Stack Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <string.h>
#include "os/os.h"

/* Create a memory pool for testing */
#define NUM_MEM_BLOCKS  (10)
#define MEM_BLOCK_SIZE  (80)

/* Limit max blocks for testing */
#define MEMPOOL_TEST_MAX_BLOCKS     (65536)

#if OS_CFG_ALIGNMENT == OS_CFG_ALIGN_4
int alignment = 4;
#else
int alignment = 8;
#endif

/* Test memory pool structure */
struct os_mempool g_TstMempool;

/* Test memory pool buffer */
os_membuf_t TstMembuf[OS_MEMPOOL_SIZE(NUM_MEM_BLOCKS, MEM_BLOCK_SIZE)];

/* Array of block pointers. */
void *block_array[MEMPOOL_TEST_MAX_BLOCKS];

int verbose = 0;

static int
mempool_test_get_pool_size(int num_blocks, int block_size)
{
    int mem_pool_size;

#if OS_CFG_ALIGNMENT == OS_CFG_ALIGN_4
    mem_pool_size = (num_blocks * ((block_size + 3)/4) * sizeof(os_membuf_t));
#else
    mem_pool_size = (num_blocks * ((block_size + 7)/8) * sizeof(os_membuf_t));
#endif

    return mem_pool_size;
}

static int
mempool_test(int num_blocks, int block_size)
{
    int cnt;
    int true_block_size;
    int mem_pool_size;
    uint8_t *tstptr;
    void **free_ptr;
    void *block;
    os_error_t rc;

    /* Check for too many blocks */
    if (num_blocks > MEMPOOL_TEST_MAX_BLOCKS) {
        printf("Error:Too many blocks to be alloated\n");
        goto err_exit;
    }

    printf("Memory pool testing (alignment=%d)\n", alignment);

    rc = os_mempool_init(&g_TstMempool, num_blocks, MEM_BLOCK_SIZE, 
                         &TstMembuf[0], "TestMemPool");
    if (rc) {
        printf("Error creating memory pool %d\n", rc);
        goto err_exit;
    } 

    if (g_TstMempool.mp_num_free != num_blocks) {
        printf("Error: number of free blocks not equal to total blocks!\n");
        goto err_exit;
    }

    if (SLIST_FIRST(&g_TstMempool) != (void *)&TstMembuf[0]) {
        printf("Error: free list pointer does not point to first block!\n");
        goto err_exit;
    }

    mem_pool_size = mempool_test_get_pool_size(num_blocks, block_size);
    if (mem_pool_size != sizeof(TstMembuf)) {
        printf("Error: total memory pool size not correct! (%d vs %lu)\n",
               mem_pool_size, sizeof(TstMembuf));
        goto err_exit;
    }

    /* Get the real block size */
#if (OS_CFG_ALIGNMENT == OS_CFG_ALIGN_4)
    true_block_size = (g_TstMempool.mp_block_size + 3) & ~3;
#else
    true_block_size = (g_TstMempool.mp_block_size + 7) & ~7;
#endif

    printf("\tMemory pool '%s' created\n", g_TstMempool.name);
    printf("\t\tmemory buffer address=%p\n", &TstMembuf[0]);
    printf("\t\tblocks=%d\n", g_TstMempool.mp_num_blocks);
    printf("\t\tblock_size=%d\n", g_TstMempool.mp_block_size);
    printf("\t\ttrue block_size=%d\n", true_block_size);
    printf("\t\tfree list ptr=%p\n", SLIST_FIRST(&g_TstMempool));
    printf("\t\ttotal size=%d bytes\n", 
           true_block_size * g_TstMempool.mp_num_blocks);

    /* Traverse free list. Better add up to number of blocks! */
    cnt = 0;
    free_ptr = (void **)&TstMembuf;
    tstptr = (uint8_t *)&TstMembuf;
    while (1) {
        /* Increment # of elements by 1 */
        ++cnt;

        /* If the free list is NULL, leave */
        if (*free_ptr == NULL) {
            break;
        }

        if (((uint8_t *)*free_ptr - (uint8_t *)free_ptr) != true_block_size) {
            printf("Free pointers are more than one block apart!");
            goto err_exit;
        }

        /* Move to next memory block */
        tstptr += true_block_size;

        if (verbose) {
            printf("free_ptr=%p\n",*free_ptr);
        }

        if (*free_ptr != (void *)tstptr) {
            printf("Error: free_ptr=%p testptr=%p\n", *free_ptr, tstptr);
            goto err_exit;
        }
        free_ptr = *free_ptr;
    }

    /* Last one in list better be NULL */
    if (cnt != g_TstMempool.mp_num_blocks) {
        printf("Error: Free list contains too many elements (%u)\n", cnt);
        goto err_exit;
    }

    /* Get a block */
    block = os_memblock_get(&g_TstMempool);
    if (block == NULL) {
        printf("Error: get block fails when pool should have elements\n");
        goto err_exit;
    }

    printf("\tObtained block %p\n", block);
    if (g_TstMempool.mp_num_free != (num_blocks-1)) {
        printf("Error: number of free blocks incorrect (%u vs %u)\n",
               g_TstMempool.mp_num_free, (num_blocks-1));
        goto err_exit;
    }

    /* Put back the block */
    printf("\tPutting back block %p\n", block);
    rc = os_memblock_put(&g_TstMempool, block);
    if (rc) {
        printf("Error: put block fails with error code=%d\n", rc);
        goto err_exit;
    }

    if (g_TstMempool.mp_num_free != num_blocks) {
        printf("Error: number of free blocks incorrect (%u vs %u)\n",
               g_TstMempool.mp_num_free, num_blocks);
        goto err_exit;
    }

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

    if ((cnt != g_TstMempool.mp_num_blocks) || 
            (cnt == MEMPOOL_TEST_MAX_BLOCKS)) {
        printf("Error: got more blocks than mempool contains (%d vs %d)\n", 
               cnt, g_TstMempool.mp_num_blocks);
    }

    /* Better be no free blocks left! */
    if (g_TstMempool.mp_num_free != 0) {
        printf("Error: got all blocks but number free not zero!\n");
        goto err_exit;
    }

    printf("\tRemoving all blocks (got %d)\n", cnt);

    /* Now put them all back */
    for (cnt = 0; cnt < g_TstMempool.mp_num_blocks; ++cnt) {
        rc = os_memblock_put(&g_TstMempool, block_array[cnt]);
        if (rc) {
            printf("Error putting back block %p (cnt=%d err=%d)", 
                   block_array[cnt], cnt, rc);
        }
    }

    /* Better be no free blocks left! */
    if (g_TstMempool.mp_num_free != g_TstMempool.mp_num_blocks) {
        printf("Error: put all blocks but number free not equal to total!\n");
        goto err_exit;
    }
    printf("\tPut all blocks back (%d)\n", g_TstMempool.mp_num_free);

    /* Better get error when we try these things! */
    rc = os_memblock_put(NULL, block_array[0]);
    if (!rc) {
        printf("Error: should have got an error trying to put to null pool\n");
    }
    rc = os_memblock_put(&g_TstMempool, NULL);
    if (!rc) {
        printf("Error: bo error trying to put to NULL block\n");
    }
    if (os_memblock_get(NULL) != NULL) {
        printf("Error: no error trying to get a block from NULL pool\n");
    }

    printf("\n");
    return 0;

err_exit:
    printf("Exit testing w/error\n");
    return -1;
}

/**
 * os mempool test 
 *  
 * Main test loop for memory pool testing. 
 * 
 * @return int 
 */
int os_mempool_test(void)
{
    int rc;

    rc = mempool_test(NUM_MEM_BLOCKS, MEM_BLOCK_SIZE);

    return rc;
}

