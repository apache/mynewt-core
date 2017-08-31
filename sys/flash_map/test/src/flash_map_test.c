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
#include <stdio.h>
#include <string.h>

#include "sysinit/sysinit.h"
#include "syscfg/syscfg.h"
#include "sysflash/sysflash.h"
#include "os/os.h"
#include "testutil/testutil.h"
#include "flash_map/flash_map.h"
#include "hal/hal_bsp.h"
#include "hal/hal_flash.h"
#include "hal/hal_flash_int.h"

struct flash_area *fa_sectors;

#if MYNEWT_VAL(SELFTEST)
/*
 * Max number sectors per area (for native BSP)
 */
#define SELFTEST_FA_SECTOR_COUNT    64
#endif

TEST_CASE_DECL(flash_map_test_case_1)
TEST_CASE_DECL(flash_map_test_case_2)

TEST_SUITE(flash_map_test_suite)
{
    flash_map_test_case_1();
    flash_map_test_case_2();
}

#if MYNEWT_VAL(SELFTEST)
int
main(int argc, char **argv)
{
    sysinit();

    fa_sectors = (struct flash_area *)
                malloc(sizeof(struct flash_area) * SELFTEST_FA_SECTOR_COUNT);
    TEST_ASSERT_FATAL(fa_sectors);

    flash_map_test_suite();

    return tu_any_failed;
}
#endif
