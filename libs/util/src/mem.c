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

#include "os/os.h"

/**
 * Mallocs a block of memory and initializes a mempool to use it.
 *
 * @param mempool               The mempool to initialize.
 * @param num_blocks            The total number of memory blocks in the
 *                                  mempool.
 * @param block_size            The size of each mempool entry.
 * @param name                  The name to give the mempool.
 * @param out_buf               On success, this points to the malloced memory.
 *                                  Pass NULL if you don't need this
 *                                  information.
 *
 * @return                      0 on success;
 *                              OS_ENOMEM on malloc failure;
 *                              Other OS code on unexpected error.
 */
int
mem_malloc_mempool(struct os_mempool *mempool, int num_blocks, int block_size,
                   char *name, void **out_buf)
{
    void *buf;
    int rc;

    block_size = OS_ALIGN(block_size, OS_ALIGNMENT);

    if (num_blocks > 0) {
        buf = malloc(OS_MEMPOOL_BYTES(num_blocks, block_size));
        if (buf == NULL) {
            return OS_ENOMEM;
        }
    } else {
        buf = NULL;
    }

    rc = os_mempool_init(mempool, num_blocks, block_size, buf, name);
    if (rc != 0) {
        free(buf);
        return rc;
    }

    if (out_buf != NULL) {
        *out_buf = buf;
    }

    return 0;
}

/**
 * Mallocs a block of memory and initializes an mbuf pool to use it.  The
 * specified block_size indicates the size of an mbuf acquired from the pool if
 * it does not contain a pkthdr.
 *
 * @param mempool               The mempool to initialize.
 * @param mbuf_pool             The mbuf pool to initialize.
 * @param num_blocks            The total number of mbufs in the pool.
 * @param block_size            The size of each mbuf.
 * @param name                  The name to give the mempool.
 * @param out_buf               On success, this points to the malloced memory.
 *                                  Pass NULL if you don't need this
 *                                  information.
 *
 * @return                      0 on success;
 *                              OS_ENOMEM on malloc failure;
 *                              Other OS code on unexpected error.
 */
int
mem_malloc_mbuf_pool(struct os_mempool *mempool,
                     struct os_mbuf_pool *mbuf_pool, int num_blocks,
                     int block_size, char *name,
                     void **out_buf)
{
    void *buf;
    int rc;

    block_size = OS_ALIGN(block_size + sizeof (struct os_mbuf), OS_ALIGNMENT);

    rc = mem_malloc_mempool(mempool, num_blocks, block_size, name, &buf);
    if (rc != 0) {
        return rc;
    }

    rc = os_mbuf_pool_init(mbuf_pool, mempool, block_size, num_blocks);
    if (rc != 0) {
        free(buf);
        return rc;
    }

    if (out_buf != NULL) {
        *out_buf = buf;
    }

    return 0;
}

/**
 * Mallocs a block of memory and initializes an mbuf pool to use it.  The
 * specified block_size indicates the size of an mbuf acquired from the pool if
 * it contains a pkthdr.
 *
 * @param mempool               The mempool to initialize.
 * @param mbuf_pool             The mbuf pool to initialize.
 * @param num_blocks            The total number of mbufs in the pool.
 * @param block_size            The size of each mbuf.
 * @param name                  The name to give the mempool.
 * @param out_buf               On success, this points to the malloced memory.
 *                                  Pass NULL if you don't need this
 *                                  information.
 *
 * @return                      0 on success;
 *                              OS_ENOMEM on malloc failure;
 *                              Other OS code on unexpected error.
 */
int
mem_malloc_mbufpkt_pool(struct os_mempool *mempool,
                        struct os_mbuf_pool *mbuf_pool, int num_blocks,
                        int block_size, char *name,
                        void **out_buf)
{
    int rc;

    rc = mem_malloc_mbuf_pool(mempool, mbuf_pool, num_blocks,
                              block_size + sizeof (struct os_mbuf_pkthdr),
                              name, out_buf);
    return rc;
}
