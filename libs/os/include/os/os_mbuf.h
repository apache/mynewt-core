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

#ifndef _OS_MBUF_H 
#define _OS_MBUF_H 

struct os_mbuf_pool {
    uint16_t omp_hdr_size;
    uint16_t omp_buf_size;
    uint16_t omp_buf_count;
    struct os_mempool *omp_pool;
};

struct os_mbuf {
    uint16_t om_flags;
    uint16_t om_len;

    SLIST_ENTRY(os_mbuf) om_next;

    uint8_t om_data[0];
};

/* Initialize a mbuf pool */
int os_mbuf_pool_init(struct os_mbuf_pool *, struct os_mempool *mp, uint16_t, 
        uint16_t, uint16_t);

/* Allocate a new mbuf out of the os_mbuf_pool */ 
struct os_mbuf *os_mbuf_get(struct os_mbuf_pool *omp, uint16_t data_start);
/* Free a mbuf */
int os_mbuf_free(struct os_mbuf_pool *omp, struct os_mbuf *mb);

#endif /* _OS_MBUF_H */ 
