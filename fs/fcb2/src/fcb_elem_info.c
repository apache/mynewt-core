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
#include <crc/crc16.h>

#include "fcb/fcb.h"
#include "fcb_priv.h"

/*
 * Given offset in flash area, compute crc16 over the data.
 */
int
fcb_elem_crc16(struct fcb_entry *loc, uint16_t *c16p)
{
    uint8_t tmp_str[FCB_TMP_BUF_SZ];
    int blk_sz;
    uint16_t crc16;
    uint32_t off;
    uint32_t end;
    int rc;

    crc16 = 0xFFFF;

    off = loc->fe_data_off;
    end = loc->fe_data_off + loc->fe_data_len;
    for (; off < end; off += blk_sz) {
        blk_sz = end - off;
        if (blk_sz > sizeof(tmp_str)) {
            blk_sz = sizeof(tmp_str);
        }

        rc = fcb_read_from_sector(loc, off, tmp_str, blk_sz);
        if (rc) {
            return FCB_ERR_FLASH;
        }
        crc16 = crc16_ccitt(crc16, tmp_str, blk_sz);
    }
    *c16p = crc16;

    return 0;
}

int
fcb_read_entry(struct fcb_entry *loc)
{
    uint8_t buf[FCB_ENTRY_SIZE];
    uint8_t entry_crc;
    uint32_t entry_offset;
    uint32_t offset;
    uint16_t len;
    int rc;

    assert(loc != NULL);
    entry_offset = fcb_entry_location_in_range(loc);
    rc = flash_area_read_is_empty(&loc->fe_range->fsr_flash_area,
        entry_offset, buf, sizeof(buf));
    if (rc < 0) {
        /* Error reading from flash */
        return FCB_ERR_FLASH;
    } else if (rc == 1) {
        /* Entry not filled on flash */
        return FCB_ERR_NOVAR;
    }
    /* Check entry CRC first */
    entry_crc = crc8_calc(crc8_init(), buf, FCB_ENTRY_SIZE - 1);
    if (entry_crc != buf[FCB_ENTRY_SIZE - 1]) {
        return FCB_ERR_CRC;
    }
    offset = (buf[0] << 16) | (buf[1] << 8) | (buf[2] << 0);
    len = (buf[3] << 8) | (buf[4] << 0);
    /* Sanity check for entry */
    if (offset < fcb_len_in_flash(loc->fe_range, sizeof(struct fcb_disk_area)) ||
        len > FCB_MAX_LEN ||
        offset + len > entry_offset) {
        /* Entry was found but data stored does not make any sense
         * report as CRC error so it can be skipped */
        return FCB_ERR_CRC;
    }

    /* Entry looks decent, pass to the caller */
    loc->fe_data_off = offset;
    loc->fe_data_len = len;

    return 0;
}

int
fcb_elem_info(struct fcb_entry *loc)
{
    int rc;
    uint16_t crc16;
    uint8_t fl_crc16[2];

    /* Read entry from the end of the sector */
    rc = fcb_read_entry(loc);
    if (rc != 0) {
        return rc;
    }

    /* Read actual data and calculate CRC */
    rc = fcb_elem_crc16(loc, &crc16);
    if (rc) {
        return rc;
    }

    /* Read CRC from flash */
    rc = fcb_read_from_sector(loc,
        loc->fe_data_off + fcb_len_in_flash(loc->fe_range, loc->fe_data_len),
        &fl_crc16, 2);
    if (rc || get_be16(fl_crc16) != crc16) {
        return FCB_ERR_CRC;
    }

    return 0;
}
