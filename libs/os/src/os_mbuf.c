/**
 * Copyright (c) Runtime Inc.
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

/*
 * Software in this file is based heavily on code written in the FreeBSD source
 * code repostiory.  While the code is written from scratch, it contains 
 * many of the ideas and logic flow in the original source, this is a 
 * derivative work, and the following license applies as well: 
 *
 * Copyright (c) 1982, 1986, 1988, 1991, 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


#include "os/os.h"

#include <string.h>

/**
 * Initialize a pool of mbufs. 
 * 
 * @param omp     The mbuf pool to initialize 
 * @param mp      The memory pool that will hold this mbuf pool 
 * @param hdr_len The length of the header, in a header mbuf 
 * @param buf_len The length of the buffer itself. 
 * @param nbufs   The number of buffers in the pool 
 *
 * @return 0 on success, error code on failure. 
 */
int 
os_mbuf_pool_init(struct os_mbuf_pool *omp, struct os_mempool *mp, 
        uint16_t hdr_len, uint16_t buf_len, uint16_t nbufs)
{
    omp->omp_hdr_len = hdr_len;
    omp->omp_databuf_len = buf_len - sizeof(struct os_mbuf);
    omp->omp_mbuf_count = nbufs;
    omp->omp_pool = mp;

    return (0);
}

/** 
 * Get an mbuf from the mbuf pool.  The mbuf is allocated, and initialized
 * prior to being returned.
 *
 * @param omp The mbuf pool to return the packet from 
 * @param leadingspace The amount of leadingspace to put before the data 
 *     section by default.
 *
 * @return An initialized mbuf on success, and NULL on failure.
 */
struct os_mbuf * 
os_mbuf_get(struct os_mbuf_pool *omp, uint16_t leadingspace)
{
    struct os_mbuf *om; 

    om = os_memblock_get(omp->omp_pool);
    if (!om) {
        goto err;
    }

    SLIST_NEXT(om, om_next) = NULL;
    om->om_flags = 0;
    om->om_len = 0;
    om->om_data = (&om->om_databuf[0] + leadingspace);

    return (om);
err:
    return (NULL);
}

/**
 * Release a mbuf back to the pool
 *
 * @param omp The Mbuf pool to release back to 
 * @param om  The Mbuf to release back to the pool 
 *
 * @return 0 on success, -1 on failure 
 */
int 
os_mbuf_free(struct os_mbuf_pool *omp, struct os_mbuf *om) 
{
    int rc;

    rc = os_memblock_put(omp->omp_pool, om);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

/**
 * Free a chain of mbufs
 *
 * @param omp The mbuf pool to free the chain of mbufs into
 * @param om  The starting mbuf of the chain to free back into the pool 
 *
 * @return 0 on success, -1 on failure 
 */
int 
os_mbuf_free_chain(struct os_mbuf_pool *omp, struct os_mbuf *om)
{
    struct os_mbuf *next;
    int rc;

    while (om != NULL) {
        next = SLIST_NEXT(om, om_next);

        rc = os_mbuf_free(omp, om);
        if (rc != 0) {
            goto err;
        }

        om = next;
    }

    return (0);
err:
    return (rc);
}

/** 
 * Copy a packet header from one mbuf to another.
 *
 * @param omp The mbuf pool associated with these buffers
 * @param new_buf The new buffer to copy the packet header into 
 * @param old_buf The old buffer to copy the packet header from
 */
static inline void 
_os_mbuf_copypkthdr(struct os_mbuf_pool *omp, struct os_mbuf *new_buf, 
        struct os_mbuf *old_buf)
{
    memcpy(&new_buf->om_databuf[0], &old_buf->om_databuf[0], 
            sizeof(struct os_mbuf_pkthdr) + omp->omp_hdr_len);
}

/** 
 * Append data onto a mbuf 
 *
 * @param omp  The mbuf pool this mbuf was allocated out of 
 * @param om   The mbuf to append the data onto 
 * @param data The data to append onto the mbuf 
 * @param len  The length of the data to append 
 * 
 * @return 0 on success, and an error code on failure 
 */
int 
os_mbuf_append(struct os_mbuf_pool *omp, struct os_mbuf *om, void *data,  
        uint16_t len)
{
    struct os_mbuf *last; 
    struct os_mbuf *new;
    int remainder;
    int space;
    int rc;

    if (omp == NULL || om == NULL) {
        rc = OS_EINVAL;
        goto err;
    }

    /* Scroll to last mbuf in the chain */
    last = om;
    while (SLIST_NEXT(last, om_next) != NULL) {
        last = SLIST_NEXT(last, om_next);
    }

    remainder = len;
    space = OS_MBUF_TRAILINGSPACE(omp, last);

    /* If room in current mbuf, copy the first part of the data into the 
     * remaining space in that mbuf.
     */
    if (space > 0) {
        if (space > remainder) {
            space = remainder;
        }

        memcpy(OS_MBUF_DATA(last, void *), data, space);

        last->om_len += space;
        data += space;
        remainder -= space;
    }

    /* Take the remaining data, and keep allocating new mbufs and copying 
     * data into it, until data is exhausted.
     */
    while (remainder > 0) {
        new = os_mbuf_get(omp, OS_MBUF_START_OFF(omp)); 
        if (!new) {
            break;
        }

        new->om_len = min(omp->omp_databuf_len, remainder);
        memcpy(OS_MBUF_DATA(om, void *), data, new->om_len);
        data += new->om_len;
        remainder -= new->om_len;
        SLIST_NEXT(last, om_next) = new;
        last = new;
    }

    /* Adjust the packet header length in the buffer */
    if (OS_MBUF_IS_PKTHDR(om)) {
        OS_MBUF_PKTHDR(om)->omp_len += len - remainder;
    }

    if (remainder != 0) {
        rc = OS_ENOMEM;
        goto err;
    } 


    return (0);
err:
    return (rc); 
}


/**
 * Duplicate a chain of mbufs.  Return the start of the duplicated chain.
 *
 * @param omp The mbuf pool to duplicate out of 
 * @param om  The mbuf chain to duplicate 
 *
 * @return A pointer to the new chain of mbufs 
 */
struct os_mbuf *
os_mbuf_dup(struct os_mbuf_pool *omp, struct os_mbuf *om)
{
    struct os_mbuf *head;
    struct os_mbuf *copy; 

    head = NULL;
    copy = NULL;

    for (; om != NULL; om = SLIST_NEXT(om, om_next)) {
        if (head) {
            SLIST_NEXT(copy, om_next) = os_mbuf_get(omp, 
                    OS_MBUF_LEADINGSPACE(omp, om)); 
            if (!SLIST_NEXT(copy, om_next)) {
                os_mbuf_free_chain(omp, head);
                goto err;
            }

            copy = SLIST_NEXT(copy, om_next);
        } else {
            head = os_mbuf_get(omp, OS_MBUF_LEADINGSPACE(omp, om));
            if (!head) {
                goto err;
            }

            if (om->om_flags & OS_MBUF_F_MASK(OS_MBUF_F_PKTHDR)) {
                _os_mbuf_copypkthdr(omp, head, om);
            }
            copy = head;
        }
        copy->om_flags = om->om_flags;
        copy->om_len = om->om_len;
        memcpy(OS_MBUF_DATA(copy, uint8_t *), OS_MBUF_DATA(om, uint8_t *),
                om->om_len);
    }

    return (head);
err:
    return (NULL);
}

#if 0

/**
 * Rearrange a mbuf chain so that len bytes are contiguous, 
 * and in the data area of an mbuf (so that OS_MBUF_DATA() will 
 * work on a structure of size len.)  Returns the resulting 
 * mbuf chain on success, free's it and returns NULL on failure.
 *
 * If there is room, it will add up to "max_protohdr - len" 
 * extra bytes to the contiguous region, in an attempt to avoid being
 * called next time.
 *
 * @param omp The mbuf pool to take the mbufs out of 
 * @param om The mbuf chain to make contiguous
 * @param len The number of bytes in the chain to make contiguous
 *
 * @return The contiguous mbuf chain on success, NULL on failure.
 */
struct os_mbuf *
os_mbuf_pullup(struct os_mbuf_pool *omp, struct os_mbuf *om, uint16_t len)
{
    struct os_mbuf *newm;

    if (len > omp->omp_databuf_len) {
        goto err;
    }

    /* Is 'n' bytes already contiguous? */
    if (((uint8_t *) &om->om_databuf[0] + OS_MBUF_END_OFF(omp)) - 
            OS_MBUF_DATA(om, uint8_t *) >= len) {
        newm = om;
        goto done;
    }

    /* Nope, OK. Allocate a new buffer, and then go through and copy 'n' 
     * bytes into that buffer.
     */
    newm = os_mbuf_get(omp, OS_MBUF_START_OFF(omp));
    if (!newm) {
        goto err;
    }

    written = 0; 
    while (written < len


done:
    return (newm);
err:
    if (om) {
        os_mbuf_free_chain(om);
    }

    return (NULL);
}
#endif
