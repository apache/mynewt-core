/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <errno.h>
#include <assert.h>
#include <os/os.h>
#include "pbuf.h"
#include "utils.h"

/* Helper funciton for getting numer of bytes being written to the bufer. */
static uint32_t
idx_occupied(uint32_t len, uint32_t wr_idx, uint32_t rd_idx)
{
    /* It is implicitly assumed wr_idx and rd_idx cannot differ by more then len. */
    return (rd_idx > wr_idx) ? (len - (rd_idx - wr_idx)) : (wr_idx - rd_idx);
}

/* Helper function for wrapping the index from the begging if above buffer len. */
static uint32_t
idx_wrap(uint32_t len, uint32_t idx)
{
    return (idx >= len) ? (idx % len) : (idx);
}

static int
validate_cfg(const struct pbuf_cfg *cfg)
{
    /* Validate pointers. */
    if (!cfg || !cfg->rd_idx_loc || !cfg->wr_idx_loc || !cfg->data_loc) {
        return -EINVAL;
    }

    /* Validate pointer alignment. */
    if (!IS_PTR_ALIGNED_BYTES(cfg->rd_idx_loc, MAX(cfg->dcache_alignment, _PBUF_IDX_SIZE)) ||
        !IS_PTR_ALIGNED_BYTES(cfg->wr_idx_loc, MAX(cfg->dcache_alignment, _PBUF_IDX_SIZE)) ||
        !IS_PTR_ALIGNED_BYTES(cfg->data_loc, _PBUF_IDX_SIZE)) {
        return -EINVAL;
    }

    /* Validate len. */
    if (cfg->len < _PBUF_MIN_DATA_LEN || !IS_PTR_ALIGNED_BYTES(cfg->len, _PBUF_IDX_SIZE)) {
        return -EINVAL;
    }

    /* Validate pointer values. */
    if (!(cfg->rd_idx_loc < cfg->wr_idx_loc) ||
        !((uint8_t *)cfg->wr_idx_loc < cfg->data_loc) ||
        !(((uint8_t *)cfg->rd_idx_loc + MAX(_PBUF_IDX_SIZE, cfg->dcache_alignment)) ==
          (uint8_t *)cfg->wr_idx_loc)) {
        return -EINVAL;
    }

    return 0;
}

int
pbuf_init(struct pbuf *pb)
{
    if (validate_cfg(&pb->cfg) != 0) {
        return -EINVAL;
    }

    /* Initialize local copy of indexes. */
    pb->data.wr_idx = 0;
    pb->data.rd_idx = 0;

    /* Clear shared memory. */
    *(pb->cfg.wr_idx_loc) = pb->data.wr_idx;
    *(pb->cfg.rd_idx_loc) = pb->data.rd_idx;

    return 0;
}

int
pbuf_write(struct pbuf *pb, const char *data, uint16_t len)
{
    if (pb == NULL || len == 0 || data == NULL) {
        /* Incorrect call. */
        return -EINVAL;
    }

    uint8_t *const data_loc = pb->cfg.data_loc;
    const uint32_t blen = pb->cfg.len;
    uint32_t rd_idx = *(pb->cfg.rd_idx_loc);
    uint32_t wr_idx = pb->data.wr_idx;

    /* wr_idx must always be aligned. */
    assert(IS_PTR_ALIGNED_BYTES(wr_idx, _PBUF_IDX_SIZE));
    /* rd_idx shall always be aligned, but its value is received from the reader.
     * Can not assert.
     */
    if (!IS_PTR_ALIGNED_BYTES(rd_idx, _PBUF_IDX_SIZE)) {
        return -EINVAL;
    }

    assert(wr_idx < blen);
    uint32_t free_space = blen - idx_occupied(blen, wr_idx, rd_idx) - _PBUF_IDX_SIZE;

    /* Packet length, data + packet length size. */
    uint32_t plen = len + PBUF_PACKET_LEN_SZ;

    /* Check if packet will fit into the buffer. */
    if (free_space < plen) {
        return -ENOMEM;
    }

    /* Clear packet len with zeros and update. Clearing is done for possible versioning in the
     * future. Writing is allowed now, because shared wr_idx value is updated at the very end.
     */
    *((uint32_t *)(&data_loc[wr_idx])) = 0;
    put_be16(&data_loc[wr_idx], len);

    wr_idx = idx_wrap(blen, wr_idx + PBUF_PACKET_LEN_SZ);

    /* Write until end of the buffer, if data will be wrapped. */
    uint32_t tail = MIN(len, blen - wr_idx);

    memcpy(&data_loc[wr_idx], data, tail);

    if (len > tail) {
        /* Copy remaining data to buffer front. */
        memcpy(&data_loc[0], data + tail, len - tail);
    }

    wr_idx = idx_wrap(blen, ROUND_UP(wr_idx + len, _PBUF_IDX_SIZE));
    /* Update wr_idx. */
    pb->data.wr_idx = wr_idx;
    *(pb->cfg.wr_idx_loc) = wr_idx;

    return len;
}

int
pbuf_read(struct pbuf *pb, char *buf, uint16_t len)
{
    if (pb == NULL) {
        /* Incorrect call. */
        return -EINVAL;
    }

    uint8_t *const data_loc = pb->cfg.data_loc;
    const uint32_t blen = pb->cfg.len;
    uint32_t wr_idx = *(pb->cfg.wr_idx_loc);
    uint32_t rd_idx = pb->data.rd_idx;

    /* rd_idx must always be aligned. */
    assert(IS_PTR_ALIGNED_BYTES(rd_idx, _PBUF_IDX_SIZE));
    /* wr_idx shall always be aligned, but its value is received from the
     * writer. Can not assert.
     */
    if (!IS_PTR_ALIGNED_BYTES(wr_idx, _PBUF_IDX_SIZE)) {
        return -EINVAL;
    }

    if (rd_idx == wr_idx) {
        /* Buffer is empty. */
        return 0;
    }

    /* Get packet len.*/
    uint16_t plen = get_be16(&data_loc[rd_idx]);

    if (!buf) {
        return (int)plen;
    }

    if (plen > len) {
        return -ENOMEM;
    }

    uint32_t occupied_space = idx_occupied(blen, wr_idx, rd_idx);

    if (occupied_space < plen + PBUF_PACKET_LEN_SZ) {
        /* This should never happen. */
        return -EAGAIN;
    }

    rd_idx = idx_wrap(blen, rd_idx + PBUF_PACKET_LEN_SZ);

    /* Packet will fit into provided buffer, truncate len if provided len
     * is bigger than necessary.
     */
    len = MIN(plen, len);

    /* Read until end of the buffer, if data are wrapped. */
    uint32_t tail = MIN(blen - rd_idx, len);

    memcpy(buf, &data_loc[rd_idx], tail);

    if (len > tail) {
        memcpy(&buf[tail], &pb->cfg.data_loc[0], len - tail);
    }

    /* Update rd_idx. */
    rd_idx = idx_wrap(blen, ROUND_UP(rd_idx + len, _PBUF_IDX_SIZE));

    pb->data.rd_idx = rd_idx;
    *(pb->cfg.rd_idx_loc) = rd_idx;

    return len;
}
