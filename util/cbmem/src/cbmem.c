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

#include <string.h>
#include "os/mynewt.h"
#include "cbmem/cbmem.h"

typedef void (copy_data_func_t) (void *dst, void *data, uint16_t len);

int
cbmem_init(struct cbmem *cbmem, void *buf, uint32_t buf_len)
{
    os_mutex_init(&cbmem->c_lock);

    memset(cbmem, 0, sizeof(*cbmem));
    cbmem->c_buf = buf;
    cbmem->c_buf_end = buf + buf_len;

    return (0);
}

int
cbmem_lock_acquire(struct cbmem *cbmem)
{
    int rc;

    if (!os_started()) {
        return (0);
    }

    rc = os_mutex_pend(&cbmem->c_lock, OS_WAIT_FOREVER);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

int
cbmem_lock_release(struct cbmem *cbmem)
{
    int rc;

    if (!os_started()) {
        return (0);
    }

    rc = os_mutex_release(&cbmem->c_lock);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}


static int
cbmem_append_internal(struct cbmem *cbmem, void *data, uint16_t len,
                      copy_data_func_t *copy_func)
{
    struct cbmem_entry_hdr *dst;
    uint8_t *start;
    uint8_t *end;
    int rc;

    rc = cbmem_lock_acquire(cbmem);
    if (rc != 0) {
        goto err;
    }

    if (cbmem->c_entry_end) {
        dst = CBMEM_ENTRY_NEXT(cbmem->c_entry_end);
    } else {
        dst = (struct cbmem_entry_hdr *) cbmem->c_buf;
    }
    end = (uint8_t *) dst + len + sizeof(*dst);

    /* If this item would take us past the end of this buffer, then adjust
     * the item to the beginning of the buffer.
     */
    if (end > cbmem->c_buf_end) {
        cbmem->c_buf_cur_end = (uint8_t *) dst;
        dst = (struct cbmem_entry_hdr *) cbmem->c_buf;
        end = (uint8_t *) dst + len + sizeof(*dst);
        if ((uint8_t *) cbmem->c_entry_start >= cbmem->c_buf_cur_end) {
            cbmem->c_entry_start = (struct cbmem_entry_hdr *) cbmem->c_buf;
        }
    }

    /* If the destination is prior to the start, and would overrwrite the
     * start of the buffer, move start forward until you don't overwrite it
     * anymore.
     */
    start = (uint8_t *) cbmem->c_entry_start;
    if (start && (uint8_t *) dst < start + CBMEM_ENTRY_SIZE(start) &&
            end > start) {
        while (start < end) {
            start = (uint8_t *) CBMEM_ENTRY_NEXT(start);
            if (start == cbmem->c_buf_cur_end) {
                start = cbmem->c_buf;
                break;
            }
        }
        cbmem->c_entry_start = (struct cbmem_entry_hdr *) start;
    }

    /* Copy the entry into the log
     */
    dst->ceh_len = len;
    copy_func((uint8_t *) dst + sizeof(*dst), data, len);

    cbmem->c_entry_end = dst;
    if (!cbmem->c_entry_start) {
        cbmem->c_entry_start = dst;
    }

    rc = cbmem_lock_release(cbmem);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (-1);
}

static void
copy_data_from_flat(void *dst, void *data, uint16_t len)
{
    memcpy(dst, data, len);
}

static void
copy_data_from_mbuf(void *dst, void *data, uint16_t len)
{
    struct os_mbuf *om = data;

    os_mbuf_copydata(om, 0, len, dst);
}

int
cbmem_append(struct cbmem *cbmem, void *data, uint16_t len)
{
    return cbmem_append_internal(cbmem, data, len, copy_data_from_flat);
}

int
cbmem_append_mbuf(struct cbmem *cbmem, struct os_mbuf *om)
{
    struct os_mbuf *om_tmp;
    uint16_t len = 0;

    om_tmp = om;
    while (om_tmp) {
        len += om_tmp->om_len;
        om_tmp = SLIST_NEXT(om_tmp, om_next);
    }

    return cbmem_append_internal(cbmem, om, len, copy_data_from_mbuf);
}

void
cbmem_iter_start(struct cbmem *cbmem, struct cbmem_iter *iter)
{
    iter->ci_start = cbmem->c_entry_start;
    iter->ci_cur = cbmem->c_entry_start;
    iter->ci_end = cbmem->c_entry_end;
}

struct cbmem_entry_hdr *
cbmem_iter_next(struct cbmem *cbmem, struct cbmem_iter *iter)
{
    struct cbmem_entry_hdr *hdr;

    if (iter->ci_start > iter->ci_end) {
        hdr = iter->ci_cur;
        iter->ci_cur = CBMEM_ENTRY_NEXT(iter->ci_cur);

        if ((uint8_t *) iter->ci_cur >= cbmem->c_buf_cur_end) {
            iter->ci_cur = (struct cbmem_entry_hdr *) cbmem->c_buf;
            iter->ci_start = (struct cbmem_entry_hdr *) cbmem->c_buf;
        }
    } else {
        hdr = iter->ci_cur;
        if (!iter->ci_cur) {
            goto err;
        }

        if (hdr == CBMEM_ENTRY_NEXT(iter->ci_end)) {
            hdr = NULL;
        } else {
            iter->ci_cur = CBMEM_ENTRY_NEXT(iter->ci_cur);
        }
    }

err:
    return (hdr);
}

int
cbmem_flush(struct cbmem *cbmem)
{
    int rc;

    rc = cbmem_lock_acquire(cbmem);
    if (rc != 0) {
        goto err;
    }

    cbmem->c_entry_start = NULL;
    cbmem->c_entry_end = NULL;
    cbmem->c_buf_cur_end = NULL;

    rc = cbmem_lock_release(cbmem);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

int
cbmem_read(struct cbmem *cbmem, struct cbmem_entry_hdr *hdr, void *buf,
        uint16_t off, uint16_t len)
{
    int rc;

    rc = cbmem_lock_acquire(cbmem);
    if (rc != 0) {
        goto err;
    }

    /* Only read the maximum number of bytes, if we exceed that,
     * truncate the read.
     */
    if (off + len > hdr->ceh_len) {
        len = hdr->ceh_len - off;
    }

    if (off > hdr->ceh_len) {
        rc = -1;
        cbmem_lock_release(cbmem);
        goto err;
    }

    memcpy(buf, (uint8_t *) hdr + sizeof(*hdr) + off, len);

    rc = cbmem_lock_release(cbmem);
    if (rc != 0) {
        goto err;
    }

    return (len);
err:
    return (-1);
}

int cbmem_read_mbuf(struct cbmem *cbmem, struct cbmem_entry_hdr *hdr,
                    struct os_mbuf *om, uint16_t off, uint16_t len)
{
    int rc;

    rc = cbmem_lock_acquire(cbmem);
    if (rc != 0) {
        goto err;
    }

    /* Only read the maximum number of bytes, if we exceed that,
     * truncate the read.
     */
    if (off + len > hdr->ceh_len) {
        len = hdr->ceh_len - off;
    }

    if (off > hdr->ceh_len) {
        rc = -1;
        cbmem_lock_release(cbmem);
        goto err;
    }

    rc = os_mbuf_append(om, (uint8_t *) hdr + sizeof(*hdr) + off, len);
    if (rc != 0) {
        cbmem_lock_release(cbmem);
        goto err;
    }

    cbmem_lock_release(cbmem);

    return (len);
err:
    return (-1);

}

int
cbmem_walk(struct cbmem *cbmem, cbmem_walk_func_t walk_func, void *arg)
{
    struct cbmem_entry_hdr *hdr;
    struct cbmem_iter iter;
    int rc;

    rc = cbmem_lock_acquire(cbmem);
    if (rc != 0) {
        goto err;
    }

    cbmem_iter_start(cbmem, &iter);
    while (1) {
        hdr = cbmem_iter_next(cbmem, &iter);
        if (hdr == NULL) {
            break;
        }

        rc = walk_func(cbmem, hdr, arg);
        if (rc == 1) {
            break;
        }
    }

    rc = cbmem_lock_release(cbmem);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}
