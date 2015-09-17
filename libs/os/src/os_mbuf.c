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

#include "os/os.h"

/**
 * Initialize a pool of mbuf's. 
 *
 */
int 
os_mbuf_pool_init(struct os_mbuf_pool *omp, struct os_mempool *mp, uint16_t hdr_size, 
        uint16_t buf_size, uint16_t nbufs)
{
    omp->omp_hdr_size = hdr_size;
    omp->omp_buf_size = buf_size;
    omp->omp_buf_count = nbufs;
    omp->omp_pool = mp;

    return (0);
}

struct os_mbuf * 
os_mbuf_get(struct os_mbuf_pool *omp, uint16_t data_start)
{
    struct os_mbuf *mb; 

    mb = os_memblock_get(omp->omp_pool);
    if (!mb) {
        goto err;
    }

    /** XXX: Initialize mbuf here */

    return (mb);
err:
    return (NULL);
}

int 
os_mbuf_free(struct os_mbuf_pool *omp, struct os_mbuf *mb) 
{
    int rc;

    rc = os_memblock_put(omp->omp_pool, mb);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}
