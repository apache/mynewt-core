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
 * Test that accesses outside of flash area or flash device are rejected,
 * including lengths that make offset + length wrap around.
 */
TEST_CASE_SELF(flash_map_test_case_bounds)
{
    const struct flash_area *fa;
    uint8_t buf[16];
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fa);
    TEST_ASSERT_FATAL(rc == 0, "flash_area_open() fail");

    rc = flash_area_read(fa, 16, buf, UINT32_MAX - 8);
    TEST_ASSERT(rc != 0);
    rc = flash_area_write(fa, 16, buf, UINT32_MAX - 8);
    TEST_ASSERT(rc != 0);
    rc = flash_area_erase(fa, 16, UINT32_MAX - 8);
    TEST_ASSERT(rc != 0);

    rc = flash_area_read_is_empty(fa, fa->fa_size, buf, sizeof(buf));
    TEST_ASSERT(rc < 0);
    rc = flash_area_read_is_empty(fa, fa->fa_size - 8, buf, sizeof(buf));
    TEST_ASSERT(rc < 0);
    rc = flash_area_read_is_empty(fa, 16, buf, UINT32_MAX - 8);
    TEST_ASSERT(rc < 0);
    rc = flash_area_read_is_empty(fa, 0, buf, sizeof(buf));
    TEST_ASSERT(rc >= 0);

    rc = hal_flash_read(fa->fa_device_id, fa->fa_off + 16, buf, UINT32_MAX - 8);
    TEST_ASSERT(rc != 0);
    rc = hal_flash_write(fa->fa_device_id, fa->fa_off + 16, buf, UINT32_MAX - 8);
    TEST_ASSERT(rc != 0);
    rc = hal_flash_isempty(fa->fa_device_id, fa->fa_off + 16, buf, UINT32_MAX - 8);
    TEST_ASSERT(rc < 0);

    flash_area_close(fa);
}
