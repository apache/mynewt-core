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

#include <assert.h>
#include <string.h>
#include "ffs_priv.h"
#include "ffs/ffs.h"

static void
ffs_area_set_magic(struct ffs_disk_area *disk_area)
{
    disk_area->fda_magic[0] = FFS_AREA_MAGIC0;
    disk_area->fda_magic[1] = FFS_AREA_MAGIC1;
    disk_area->fda_magic[2] = FFS_AREA_MAGIC2;
    disk_area->fda_magic[3] = FFS_AREA_MAGIC3;
}

int
ffs_area_magic_is_set(const struct ffs_disk_area *disk_area)
{
    return disk_area->fda_magic[0] == FFS_AREA_MAGIC0 &&
           disk_area->fda_magic[1] == FFS_AREA_MAGIC1 &&
           disk_area->fda_magic[2] == FFS_AREA_MAGIC2 &&
           disk_area->fda_magic[3] == FFS_AREA_MAGIC3;
}

int
ffs_area_is_scratch(const struct ffs_disk_area *disk_area)
{
    return ffs_area_magic_is_set(disk_area) &&
           disk_area->fda_id == FFS_AREA_ID_NONE;
}

void
ffs_area_to_disk(const struct ffs_area *area,
                 struct ffs_disk_area *out_disk_area)
{
    memset(out_disk_area, 0, sizeof *out_disk_area);
    ffs_area_set_magic(out_disk_area);
    out_disk_area->fda_length = area->fa_length;
    out_disk_area->fda_ver = FFS_AREA_VER;
    out_disk_area->fda_gc_seq = area->fa_gc_seq;
    out_disk_area->fda_id = area->fa_id;
}

uint32_t
ffs_area_free_space(const struct ffs_area *area)
{
    return area->fa_length - area->fa_cur;
}

/**
 * Finds a corrupt scratch area.  An area is indentified as a corrupt scratch
 * area if it and another area share the same ID.  Among two areas with the
 * same ID, the one with fewer bytes written is the corrupt scratch area.
 *
 * @param out_good_idx          On success, the index of the good area (longer
 *                                  of the two areas) gets written here.
 * @param out_bad_idx           On success, the index of the corrupt scratch
 *                                  area gets written here.
 *
 * @return                      0 if a corrupt scratch area was identified;
 *                              FFS_ENOENT if one was not found.
 */
int
ffs_area_find_corrupt_scratch(uint16_t *out_good_idx, uint16_t *out_bad_idx)
{
    const struct ffs_area *iarea;
    const struct ffs_area *jarea;
    int i;
    int j;

    for (i = 0; i < ffs_num_areas; i++) {
        iarea = ffs_areas + i;
        for (j = i + 1; j < ffs_num_areas; j++) {
            jarea = ffs_areas + j;

            if (jarea->fa_id == iarea->fa_id) {
                /* Found a duplicate.  The shorter of the two areas should be
                 * used as scratch.
                 */
                if (iarea->fa_cur < jarea->fa_cur) {
                    *out_good_idx = j;
                    *out_bad_idx = i;
                } else {
                    *out_good_idx = i;
                    *out_bad_idx = j;
                }

                return 0;
            }
        }
    }

    return FFS_ENOENT;
}
