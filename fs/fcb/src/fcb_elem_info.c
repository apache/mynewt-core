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
#include <syscfg/syscfg.h>

#if MYNEWT_VAL(FCB_CRC_8)
#include <crc/crc8.h>
#endif
#if MYNEWT_VAL(FCB_CRC_16)
#include <crc/crc16.h>
#endif

#include "fcb/fcb.h"
#include "fcb_priv.h"

#if MYNEWT_VAL(FCB_CRC_8) && MYNEWT_VAL(FCB_CRC_16)
/*
 * Need to check from FCB structure which mode it is operating at.
 */
#define FCB_CRC_INIT(fcb)                                               \
    ((fcb)->f_crc_actual == FCB_CRC_8 ? crc8_init() : CRC16_INITIAL_CRC)
#define FCB_CRC(fcb, crc, data, len)                                    \
    ((fcb)->f_crc_actual == FCB_CRC_8 ?                                 \
      crc8_calc(crc, data, len) :                                       \
      crc16_ccitt(crc, data, len))
#elif MYNEWT_VAL(FCB_CRC_8)
/*
 * crc8
 */
#define FCB_CRC_INIT(fcb) crc8_init()
#define FCB_CRC(fcb, crc, data, len)                                    \
    crc8_calc(crc, data, len)
#elif MYNEWT_VAL(FCB_CRC_16)
/*
 * crc16
 */
#define FCB_CRC_INIT(fcb) CRC16_INITIAL_CRC
#define FCB_CRC(fcb, crc, data, len)                                    \
    crc16_ccitt(crc, data, len)
#endif

/*
 * Given offset in flash area, fill in rest of the fcb_entry, and crc8 over
 * the data.
 */
int
fcb_elem_crc(struct fcb *fcb, struct fcb_entry *loc, uint16_t *cp)
{
    uint8_t tmp_str[FCB_TMP_BUF_SZ];
    int cnt;
    int blk_sz;
    uint16_t crc;
    uint16_t len;
    uint32_t off;
    uint32_t end;
    int rc;

    if (loc->fe_elem_off + 2 > loc->fe_area->fa_size) {
        return FCB_ERR_NOVAR;
    }
    rc = flash_area_read(loc->fe_area, loc->fe_elem_off, tmp_str, 2);
    if (rc) {
        return FCB_ERR_FLASH;
    }

    cnt = fcb_get_len(tmp_str, &len);
    if (cnt < 0) {
        return cnt;
    }
    loc->fe_data_off = loc->fe_elem_off + fcb_len_in_flash(fcb, cnt);
    loc->fe_data_len = len;

    crc = FCB_CRC_INIT(fcb);
    crc = FCB_CRC(fcb, crc, tmp_str, cnt);

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
        crc = FCB_CRC(fcb, crc, tmp_str, blk_sz);
    }
    if (fcb->f_crc_actual == FCB_CRC_8) {
        crc = htole16(crc);
    }
    *cp = crc;

    return 0;
}

int
fcb_elem_info(struct fcb *fcb, struct fcb_entry *loc)
{
    int rc;
    uint16_t crc;
    uint16_t fl_crc = 0;
    uint32_t off;

    rc = fcb_elem_crc(fcb, loc, &crc);
    if (rc) {
        return rc;
    }
    off = loc->fe_data_off + fcb_len_in_flash(fcb, loc->fe_data_len);

    rc = flash_area_read(loc->fe_area, off, &fl_crc, fcb->f_crc_actual);
    if (rc) {
        return FCB_ERR_FLASH;
    }

    if (fl_crc != crc) {
        return FCB_ERR_CRC;
    }
    return 0;
}
