/**
 * Copyright (c) 2015 Runtime Inc.
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
#include <string.h>

#include <hal/hal_flash.h>
#include <hal/hal_flash_int.h>
#ifdef NFFS_PRESENT
#include <nffs/nffs.h>
#endif
#include "util/flash_map.h"

static const struct flash_area *flash_map;
static int flash_map_entries;

void
flash_area_init(const struct flash_area *map, int map_entries)
{
    flash_map = map;
    flash_map_entries = map_entries;
    /*
     * XXX should we validate this against current flashes?
     */
}

int
flash_area_open(int idx, const struct flash_area **fap)
{
    if (!flash_map || idx >= flash_map_entries) {
        return -1;
    }
    *fap = &flash_map[idx];
    return 0;
}

void
flash_area_close(const struct flash_area *fa)
{
    /* nothing to do for now */
}

int
flash_area_to_sectors(int idx, int *cnt, struct flash_area *ret)
{
    int i;
    const struct hal_flash *hf;
    const struct flash_area *fa;
    uint32_t start, size;

    if (!flash_map || idx >= flash_map_entries) {
        return -1;
    }
    *cnt = 0;
    fa = &flash_map[idx];

    hf = bsp_flash_dev(fa->fa_flash_id);
    for (i = 0; i < hf->hf_sector_cnt; i++) {
        hf->hf_itf->hff_sector_info(i, &start, &size);
        if (start >= fa->fa_off && start < fa->fa_off + fa->fa_size) {
            if (ret) {
                ret->fa_flash_id = fa->fa_flash_id;
                ret->fa_off = start;
                ret->fa_size = size;
                ret++;
            }
            *cnt = *cnt + 1;
        }
    }
    return 0;
}

#ifdef NFFS_PRESENT
int
flash_area_to_nffs_desc(int idx, int *cnt, struct nffs_area_desc *nad)
{
    int i;
    const struct hal_flash *hf;
    const struct flash_area *fa;
    uint32_t start, size;

    if (!flash_map || idx >= flash_map_entries) {
        return -1;
    }
    *cnt = 0;
    fa = &flash_map[idx];

    hf = bsp_flash_dev(fa->fa_flash_id);
    for (i = 0; i < hf->hf_sector_cnt; i++) {
        hf->hf_itf->hff_sector_info(i, &start, &size);
        if (start >= fa->fa_off && start < fa->fa_off + fa->fa_size) {
            if (nad) {
                nad->nad_flash_id = fa->fa_flash_id;
                nad->nad_offset = start;
                nad->nad_length = size;
                nad++;
            }
            *cnt = *cnt + 1;
        }
    }
    if (nad) {
        memset(nad, 0, sizeof(*nad));
    }
    return 0;
}
#endif /* NFFS_PRESENT */

int
flash_area_read(const struct flash_area *fa, uint32_t off, void *dst,
  uint32_t len)
{
    if (off > fa->fa_size || off + len > fa->fa_size) {
        return -1;
    }
    return hal_flash_read(fa->fa_flash_id, fa->fa_off + off, dst, len);
}

int
flash_area_write(const struct flash_area *fa, uint32_t off, void *src,
  uint32_t len)
{
    if (off > fa->fa_size || off + len > fa->fa_size) {
        return -1;
    }
    return hal_flash_write(fa->fa_flash_id, fa->fa_off + off, src, len);
}

int
flash_area_erase(const struct flash_area *fa, uint32_t off, uint32_t len)
{
    if (off > fa->fa_size || off + len > fa->fa_size) {
        return -1;
    }
    return hal_flash_erase(fa->fa_flash_id, fa->fa_off + off, len);
}
