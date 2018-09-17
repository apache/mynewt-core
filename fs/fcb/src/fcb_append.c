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
#include <stddef.h>

#include "fcb/fcb.h"
#include "fcb_priv.h"

int
fcb_new_sector(struct fcb *fcb, int cnt)
{

    int new_sector = -1;
    int sector = fcb->f_active.fe_sector;
    do {
        sector = fcb_getnext_sector(fcb, sector);
        if (new_sector < 0) {
            new_sector = sector;
        }
        if (sector == fcb->f_oldest_sec) {
            new_sector = -1;
            break;
        }
    } while (--cnt >= 0);

    return new_sector;
}

/*
 * Take one of the scratch blocks into use, if at all possible.
 */
int
fcb_append_to_scratch(struct fcb *fcb)
{
    int sector;
    int rc;

    sector = fcb_new_sector(fcb, 0);
    if (sector < 0) {
        return FCB_ERR_NOSPACE;
    }
    rc = fcb_sector_hdr_init(fcb, sector, fcb->f_active_id + 1);
    if (rc) {
        return rc;
    }
    fcb->f_active.fe_range = fcb_get_sector_range(fcb, sector);
    fcb->f_active.fe_sector = sector;
    fcb->f_active.fe_elem_off = sizeof(struct fcb_disk_area);
    fcb->f_active_id++;
    return FCB_OK;
}

static inline int
sector_offset_to_flash_area_offset(const struct sector_range *range, int sector,
    int offset)
{
    return offset + ((sector - range->sr_first_sector) * range->sr_sector_size);
}

static inline int
fcb_entry_flash_offset(const struct fcb_entry *loc)
{
    return (loc->fe_sector - loc->fe_range->sr_first_sector) *
        loc->fe_range->sr_sector_size;
}

int
fcb_append(struct fcb *fcb, uint16_t len, struct fcb_entry *append_loc)
{
    struct fcb_entry *active;
    struct sector_range *range;
    uint8_t tmp_str[2];
    int cnt;
    int sector;
    int rc;

    cnt = fcb_put_len(tmp_str, len);
    if (cnt < 0) {
        return cnt;
    }
    cnt = fcb_len_in_flash(fcb, cnt);
    len = fcb_len_in_flash(fcb, len) + fcb_len_in_flash(fcb, FCB_CRC_SZ);

    rc = os_mutex_pend(&fcb->f_mtx, OS_WAIT_FOREVER);
    if (rc && rc != OS_NOT_STARTED) {
        return FCB_ERR_ARGS;
    }
    active = &fcb->f_active;
    if (active->fe_elem_off + len + cnt > active->fe_range->sr_sector_size) {
        sector = fcb_new_sector(fcb, fcb->f_scratch_cnt);
        if (sector >= 0) {
            range = fcb_get_sector_range(fcb, sector);
        }
        if (sector < 0 || (range->sr_sector_size <
            sizeof(struct fcb_disk_area) + len + cnt)) {
            rc = FCB_ERR_NOSPACE;
            goto err;
        }
        rc = fcb_sector_hdr_init(fcb, sector, fcb->f_active_id + 1);
        if (rc) {
            goto err;
        }
        fcb->f_active.fe_range = range;
        fcb->f_active.fe_sector = sector;
        fcb->f_active.fe_elem_off = sizeof(struct fcb_disk_area);
        fcb->f_active_id++;
    }

    rc = flash_area_write(&active->fe_range->sr_flash_area,
        sector_offset_to_flash_area_offset(active->fe_range, active->fe_sector, active->fe_elem_off),
        tmp_str, cnt);
    if (rc) {
        rc = FCB_ERR_FLASH;
        goto err;
    }
    append_loc->fe_range = active->fe_range;
    append_loc->fe_sector = active->fe_sector;
    append_loc->fe_elem_off = active->fe_elem_off;
    append_loc->fe_data_off = active->fe_elem_off + cnt;

    active->fe_elem_off = append_loc->fe_data_off + len;
    active->fe_data_off = append_loc->fe_data_off;

    os_mutex_release(&fcb->f_mtx);

    return FCB_OK;
err:
    os_mutex_release(&fcb->f_mtx);
    return rc;
}

int
fcb_append_finish(struct fcb *fcb, struct fcb_entry *loc)
{
    int rc;
    uint8_t crc8;
    uint32_t off;

    rc = fcb_elem_crc8(fcb, loc, &crc8);
    if (rc) {
        return rc;
    }
    off = loc->fe_data_off + fcb_len_in_flash(fcb, loc->fe_data_len);

    rc = flash_area_write(&loc->fe_range->sr_flash_area,
        fcb_entry_flash_offset(loc) + off, &crc8, sizeof(crc8));
    if (rc) {
        return FCB_ERR_FLASH;
    }
    return 0;
}
