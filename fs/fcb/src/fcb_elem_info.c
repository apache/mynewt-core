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

#include <crc/crc8.h>

#include "fcb/fcb.h"
#include "fcb_priv.h"

/*
 * Given offset in flash area, fill in rest of the fcb_entry, and crc8 over
 * the data.
 */
int
fcb_elem_crc8(struct fcb *fcb, struct fcb_entry *loc, uint8_t *c8p)
{
    uint8_t tmp_str[FCB_TMP_BUF_SZ];
    int cnt;
    int blk_sz;
    uint8_t crc8;
    uint16_t len;
    uint32_t off;
    uint32_t end;
    int rc;

    if (loc->fe_elem_off + 2 > loc->fe_area->fa_size) {
        return FCB_ERR_NOVAR;
    }
    rc = flash_area_read_is_empty(loc->fe_area, loc->fe_elem_off, tmp_str, 2);
    if (rc < 0) {
        return FCB_ERR_FLASH;
    } else if (rc == 1) {
        return FCB_ERR_NOVAR;
    }

    cnt = fcb_get_len(tmp_str, &len);
    loc->fe_data_off = loc->fe_elem_off + fcb_len_in_flash(fcb, cnt);
    loc->fe_data_len = len;

    crc8 = crc8_init();
    crc8 = crc8_calc(crc8, tmp_str, cnt);

    off = loc->fe_data_off;
    end = loc->fe_data_off + len;
    for (; off < end; off += blk_sz) {
        blk_sz = end - off;
        if (blk_sz > sizeof(tmp_str)) {
            blk_sz = sizeof(tmp_str);
        }

        rc = flash_area_read(loc->fe_area, off, tmp_str, blk_sz);
        if (rc) {
            return FCB_ERR_FLASH;
        }
        crc8 = crc8_calc(crc8, tmp_str, blk_sz);
    }
    *c8p = crc8;

    return 0;
}

int
fcb_elem_info(struct fcb *fcb, struct fcb_entry *loc)
{
    int rc;
    uint8_t crc8;
    uint8_t fl_crc8;
    uint32_t off;

    rc = fcb_elem_crc8(fcb, loc, &crc8);
    if (rc) {
        return rc;
    }
    off = loc->fe_data_off + fcb_len_in_flash(fcb, loc->fe_data_len);

    rc = flash_area_read(loc->fe_area, off, &fl_crc8, sizeof(fl_crc8));
    if (rc) {
        return FCB_ERR_FLASH;
    }

    if (fl_crc8 != crc8) {
        return FCB_ERR_CRC;
    }
    return 0;
}
