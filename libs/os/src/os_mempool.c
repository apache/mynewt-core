/**
 * Copyright (c) 2015 Runtime Inc.
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

#include "os/os.h"

/**
 * os mempool init
 *  
 * Initialize a memory pool. 
 * 
 * @param mp            Pointer to a pointer to a mempool
 * @param blocks        The number of blocks in the pool
 * @param blocks_size   The size of the block, in bytes. 
 * @param membuf        Pointer to memory to contain blocks. 
 * @param name          Name of the pool.
 * 
 * @return os_error_t 
 */
os_error_t
os_mempool_init(struct os_mempool *mp, int blocks, int block_size, void *membuf,
                char *name)
{
    int true_block_size;
    uint8_t *block_addr;
    struct os_memblock *block_ptr;

    /* Check for valid parameters */
    if ((!mp) || (!membuf) || (blocks <= 0) || (block_size <= 0)) {
        return OS_INVALID_PARM;
    }

    /* Blocks need to be sized properly and memory buffer should be aligned */
    if (((uint32_t)membuf & (OS_ALIGNMENT - 1)) != 0) {
        return OS_MEM_NOT_ALIGNED;
    }
    true_block_size = OS_ALIGN(block_size, OS_ALIGNMENT);

    /* Initialize the memory pool structure */
    mp->mp_block_size = block_size;
    mp->mp_num_free = blocks;
    mp->mp_num_blocks = blocks;
    mp->name = name;
    SLIST_FIRST(mp) = membuf;

    /* Chain the memory blocks to the free list */
    block_addr = (uint8_t *)membuf;
    block_ptr = (struct os_memblock *)block_addr;
    while (blocks > 1) {
        block_addr += true_block_size;
        SLIST_NEXT(block_ptr, mb_next) = (struct os_memblock *)block_addr;
        block_ptr = (struct os_memblock *)block_addr;
        --blocks;
    }

    /* Last one in the list should be NULL */
    SLIST_NEXT(block_ptr, mb_next) = NULL;

    return OS_OK;
}

/**
 * os memblock get 
 *  
 * Get a memory block from a memory pool 
 * 
 * @param mp Pointer to the memory pool
 * 
 * @return void* Pointer to block if available; NULL otherwise
 */
void *
os_memblock_get(struct os_mempool *mp)
{
    os_sr_t sr;
    struct os_memblock *block;

    /* Check to make sure they passed in a memory pool (or something) */
    block = NULL;
    if (mp) {
        OS_ENTER_CRITICAL(sr);
        /* Check for any free */
        if (mp->mp_num_free) {
            /* Get a free block */
            block = SLIST_FIRST(mp);

            /* Set new free list head */
            SLIST_FIRST(mp) = SLIST_NEXT(block, mb_next);

            /* Decrement number free by 1 */
            mp->mp_num_free--;
        }
        OS_EXIT_CRITICAL(sr);
    }

    return (void *)block;
}

/**
 * os memblock put 
 *  
 * Puts the memory block back into the pool 
 * 
 * @param mp Pointer to memory pool
 * @param block_addr Pointer to memory block
 * 
 * @return os_error_t 
 */
os_error_t
os_memblock_put(struct os_mempool *mp, void *block_addr)
{
    struct os_memblock *block;
    os_sr_t sr;

    /* Make sure parameters are valid */
    if ((mp == NULL) || (block_addr == NULL)) {
        return OS_INVALID_PARM;
    }

    /* 
     * XXX: we should do boundary checks here! The block had better be within 
     * the pool. If it fails, do we return an error or assert()? Add this when 
     * we add the memory debug. 
     */ 
    block = (struct os_memblock *)block_addr;
    OS_ENTER_CRITICAL(sr);
    
    /* Chain current free list pointer to this block; make this block head */
    SLIST_NEXT(block, mb_next) = SLIST_FIRST(mp);
    SLIST_FIRST(mp) = block;

    /* XXX: Should we check that the number free <= number blocks? */
    /* Increment number free */
    mp->mp_num_free++;

    OS_EXIT_CRITICAL(sr);

    return OS_OK;
}
