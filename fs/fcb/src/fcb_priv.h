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
#ifndef __SYS_FCB_PRIV_H_
#define __SYS_FCB_PRIV_H_

#ifdef __cplusplus
extern "C" {
#endif

#define FCB_CRC_SZ	sizeof(uint8_t)
#define FCB_TMP_BUF_SZ	32

#define FCB_ID_GT(a, b) (((int16_t)(a) - (int16_t)(b)) > 0)

struct fcb_disk_area {
    uint32_t fd_magic;
    uint8_t  fd_ver;
    uint8_t  _pad;
    uint16_t fd_id;
};

int fcb_put_len(uint8_t *buf, uint16_t len);
int fcb_get_len(uint8_t *buf, uint16_t *len);

static inline int
fcb_len_in_flash(struct fcb *fcb, uint16_t len)
{
    if (fcb->f_align <= 1) {
        return len;
    }
    return (len + (fcb->f_align - 1)) & ~(fcb->f_align - 1);
}

int fcb_getnext_in_area(struct fcb *fcb, struct fcb_entry *loc);
struct flash_area *fcb_getnext_area(struct fcb *fcb, struct flash_area *fap);

static inline int
fcb_getnext_sector(struct fcb *fcb, int sector)
{
    if (++sector >= fcb->f_sector_cnt) {
        sector = 0;
    }
    return sector;
}

static inline int
fcb_sector_flash_offset(const struct fcb_entry *loc)
{
    return (loc->fe_sector - loc->fe_range->sr_first_sector) *
        loc->fe_range->sr_sector_size;
}

int fcb_getnext_nolock(struct fcb *fcb, struct fcb_entry *loc);

int fcb_elem_info(struct fcb *, struct fcb_entry *);
int fcb_elem_crc8(struct fcb *, struct fcb_entry *loc, uint8_t *crc8p);

int fcb_sector_hdr_init(struct fcb *fcb, int sector, uint16_t id);

struct sector_range *fcb_get_sector_range(const struct fcb *fcb, int sector);

int fcb_sector_hdr_read(struct fcb *, struct sector_range *srp, uint16_t sec,
  struct fcb_disk_area *fdap);

#ifdef __cplusplus
}
#endif

#endif
