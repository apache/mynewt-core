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

static struct flash_area *
fcb_new_area(struct fcb *fcb, int cnt)
{
    struct flash_area *fa;
    struct flash_area *rfa;
    int i;

    rfa = NULL;
    i = 0;
    fa = fcb->f_active.fe_area;
    do {
        fa = fcb_getnext_area(fcb, fa);
        if (!rfa) {
            rfa = fa;
        }
        if (fa == fcb->f_oldest) {
            return NULL;
        }
    } while (i++ < cnt);
    return rfa;
}

/*
 * Take one of the scratch blocks into use, if at all possible.
 */
int
fcb_append_to_scratch(struct fcb *fcb)
{
    struct flash_area *fa;
    int rc;

    fa = fcb_new_area(fcb, 0);
    if (!fa) {
        return FCB_ERR_NOSPACE;
    }
    rc = fcb_sector_hdr_init(fcb, fa, fcb->f_active_id + 1);
    if (rc) {
        return rc;
    }
    fcb->f_active.fe_area = fa;
    fcb->f_active.fe_elem_off = fcb_len_in_flash(fcb, sizeof(struct fcb_disk_area));
    fcb->f_active_id++;
    return FCB_OK;
}

int
fcb_write(struct fcb *fcb, struct fcb_entry *loc, const uint8_t *buf, size_t len)
{
    int rc;

    rc = flash_area_write(loc->fe_area, loc->fe_data_off, buf, len);
    if (rc == 0) {
        loc->fe_data_off += len;
    }

    return rc;
}

int
fcb_append(struct fcb *fcb, uint16_t len, struct fcb_entry *append_loc)
{
    struct fcb_entry *active;
    struct flash_area *fa;
    uint8_t tmp_str[2];
    int cnt;
    int len_bytes_in_flash;
    int entry_len_in_flash;
    int rc;

    cnt = fcb_put_len(tmp_str, len);
    if (cnt < 0) {
        return cnt;
    }
    len_bytes_in_flash = fcb_len_in_flash(fcb, cnt);
    entry_len_in_flash = fcb_entry_total_len(fcb, len);

    rc = os_mutex_pend(&fcb->f_mtx, OS_WAIT_FOREVER);
    if (rc && rc != OS_NOT_STARTED) {
        return FCB_ERR_ARGS;
    }
    active = &fcb->f_active;
    if (active->fe_elem_off + entry_len_in_flash > active->fe_area->fa_size) {
        fa = fcb_new_area(fcb, fcb->f_scratch_cnt);
        if (!fa || (fa->fa_size <
                    fcb_len_in_flash(fcb, sizeof(struct fcb_disk_area)) + entry_len_in_flash)) {
            rc = FCB_ERR_NOSPACE;
            goto err;
        }
        rc = fcb_sector_hdr_init(fcb, fa, fcb->f_active_id + 1);
        if (rc) {
            goto err;
        }
        fcb->f_active.fe_area = fa;
        fcb->f_active.fe_elem_off = fcb_len_in_flash(fcb, sizeof(struct fcb_disk_area));
        fcb->f_active_id++;
        fcb->f_active_sector_entry_count = 0;
    }

    rc = flash_area_write(active->fe_area, active->fe_elem_off, tmp_str, cnt);
    if (rc) {
        rc = FCB_ERR_FLASH;
        goto err;
    }
    append_loc->fe_area = active->fe_area;
    append_loc->fe_elem_off = active->fe_elem_off;
    append_loc->fe_data_off = active->fe_elem_off + len_bytes_in_flash;
    append_loc->fe_data_len = len;

    active->fe_elem_off = append_loc->fe_elem_off + entry_len_in_flash;
    active->fe_data_off = append_loc->fe_data_off;
    active->fe_data_len = len;

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

    fcb->f_active_sector_entry_count++;
    rc = fcb_elem_crc8(fcb, loc, &crc8);
    if (rc) {
        return rc;
    }
    off = loc->fe_data_off + fcb_len_in_flash(fcb, loc->fe_data_len);

    rc = flash_area_write(loc->fe_area, off, &crc8, sizeof(crc8));
    if (rc) {
        return FCB_ERR_FLASH;
    }
    return 0;
}
