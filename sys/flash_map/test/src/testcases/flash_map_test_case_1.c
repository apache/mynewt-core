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

/*
 * Test flash_area_to_sectors()
 */
TEST_CASE(flash_map_test_case_1)
{
    const struct flash_area *fa;
    int areas_checked = 0;
    int i, j, rc;
    const struct hal_flash *hf;
    struct flash_area my_secs[32];
    int my_sec_cnt;
    uint32_t end;

    sysinit();

    for (i = 0; i < 8; i++) {
        rc = flash_area_open(i, &fa);
        if (rc) {
            continue;
        }

        hf = hal_bsp_flash_dev(fa->fa_device_id);
        TEST_ASSERT_FATAL(hf != NULL, "hal_bsp_flash_dev");

        rc = flash_area_to_sectors(i, &my_sec_cnt, my_secs);
        TEST_ASSERT_FATAL(rc == 0, "flash_area_to_sectors failed");

        end = fa->fa_off;
        for (j = 0; j < my_sec_cnt; j++) {
            TEST_ASSERT_FATAL(end == my_secs[j].fa_off, "Non contiguous area");
            TEST_ASSERT_FATAL(my_secs[j].fa_device_id == fa->fa_device_id,
              "Sectors not in same flash?");
            end = my_secs[j].fa_off + my_secs[j].fa_size;
        }
        if (my_sec_cnt) {
            areas_checked++;
            TEST_ASSERT_FATAL(my_secs[my_sec_cnt - 1].fa_off +
              my_secs[my_sec_cnt - 1].fa_size == fa->fa_off + fa->fa_size,
              "Last sector not in the end");
        }
    }
    TEST_ASSERT_FATAL(areas_checked != 0, "No flash map areas to check!");
}
