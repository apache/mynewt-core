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
#include "flash_map_test.h"

extern struct flash_area *fa_sectors;

/*
 * Test flash_area_to_sectors() vs flash_area_getnext_sector()
 */
TEST_CASE(flash_map_test_case_3)
{
    int areas[] = {
        FLASH_AREA_BOOTLOADER,
        FLASH_AREA_IMAGE_0,
        FLASH_AREA_IMAGE_1,
        FLASH_AREA_IMAGE_SCRATCH,
        FLASH_AREA_REBOOT_LOG,
        FLASH_AREA_NFFS
    };
    int i;
    int sec_cnt;
    int aid;
    int sec_idx;
    int j;
    int rc;
    struct flash_area ret;

    for (i = 0; i < sizeof(areas) / sizeof(areas[0]); i++) {
        aid = areas[i];
        rc = flash_area_to_sectors(aid, &sec_cnt, fa_sectors);
        TEST_ASSERT_FATAL(rc == 0, "flash_area_to_sectors failed");

        sec_idx = -1;
        j = 0;
        while (1) {
            rc = flash_area_getnext_sector(aid, &sec_idx, &ret);
            if (rc == 0) {
                TEST_ASSERT(fa_sectors[j].fa_device_id == ret.fa_device_id);
                TEST_ASSERT(fa_sectors[j].fa_id == ret.fa_id);
                TEST_ASSERT(fa_sectors[j].fa_off == ret.fa_off);
                TEST_ASSERT(fa_sectors[j].fa_size == ret.fa_size);
                j++;
            } else {
                TEST_ASSERT(sec_cnt == j);
                break;
            }
        }
    }
}
