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
#include <limits.h>
#include "os/mynewt.h"
#include "hal/hal_bsp.h"
#include "flash_map/flash_map.h"
#include "bootutil/image.h"
#include "imgmgr/imgmgr.h"
#include "coredump/coredump.h"

uint8_t coredump_disabled;

static void
dump_core_tlv(const struct flash_area *fa, uint32_t *off,
  struct coredump_tlv *tlv, void *data)
{
    flash_area_write(fa, *off, tlv, sizeof(*tlv));
    *off += sizeof(*tlv);

    flash_area_write(fa, *off, data, tlv->ct_len);
    *off += tlv->ct_len;
}

void
coredump_dump(void *regs, int regs_sz)
{
    struct coredump_header hdr;
    struct coredump_tlv tlv;
    const struct flash_area *fa;
    struct image_version ver;
    const struct hal_bsp_mem_dump *mem, *cur;
    int area_cnt, i;
    uint8_t hash[IMGMGR_HASH_LEN];
    uint32_t off;
    uint32_t area_off, area_end;
    int slot;

    if (coredump_disabled) {
        return;
    }
    if (flash_area_open(MYNEWT_VAL(COREDUMP_FLASH_AREA), &fa)) {
        return;
    }

    if (flash_area_read(fa, 0, &hdr, sizeof(hdr))) {
        return;
    }
    if (hdr.ch_magic == COREDUMP_MAGIC) {
        /*
         * Don't override corefile.
         */
        return;
    }

    /* Don't overwrite an image that has any flags set (pending, active, or
     * confirmed).
     */
    slot = flash_area_id_to_image_slot(MYNEWT_VAL(COREDUMP_FLASH_AREA));
    if (slot != -1) {
        if (imgmgr_state_slot_in_use(slot)) {
            return;
        }
    }

    if (flash_area_erase(fa, 0, fa->fa_size)) {
        return;
    }

    /*
     * First put in data, followed by the header.
     */
    tlv.ct_type = COREDUMP_TLV_REGS;
    tlv._pad = 0;
    tlv.ct_len = regs_sz;
    tlv.ct_off = 0;

    off = sizeof(hdr);
    dump_core_tlv(fa, &off, &tlv, regs);

    if (imgr_read_info(boot_current_slot, &ver, hash, NULL) == 0) {
        tlv.ct_type = COREDUMP_TLV_IMAGE;
        tlv.ct_len = IMGMGR_HASH_LEN;

        dump_core_tlv(fa, &off, &tlv, hash);
    }

    mem = hal_bsp_core_dump(&area_cnt);
    for (i = 0; i < area_cnt; i++) {
        cur = &mem[i];
        area_off = (uint32_t)cur->hbmd_start;
        area_end = area_off + cur->hbmd_size;
        while (area_off < area_end) {
            tlv.ct_type = COREDUMP_TLV_MEM;
            if (cur->hbmd_size > USHRT_MAX) {
                tlv.ct_len = SHRT_MAX + 1;
            } else {
                tlv.ct_len = cur->hbmd_size;
            }
            tlv.ct_off = area_off;
            dump_core_tlv(fa, &off, &tlv, (void *)area_off);
            area_off += tlv.ct_len;
        }
    }
    hdr.ch_magic = COREDUMP_MAGIC;
    hdr.ch_size = off;

    flash_area_write(fa, 0, &hdr, sizeof(hdr));
}
