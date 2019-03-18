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

#include "os/mynewt.h"
#include <flash_map/flash_map.h>
#include <testutil/testutil.h>
#include <fcb/fcb.h>
#include "enc_flash_test.h"

/*
 * Native BSP has encrypted flash driver registered with device ID 1.
 */
#define ENC_TEST_FLASH_ID	1

struct flash_area enc_test_flash_areas[4] = {
    [0] = {
        .fa_id = ENC_TEST_FLASH_ID,
        .fa_off = 0x00000000,
        .fa_size = 16 * 1024
    },
    [1] = {
        .fa_id = ENC_TEST_FLASH_ID,
        .fa_off = 0x00004000,
        .fa_size = 16 * 1024
    },
    [2] = {
        .fa_id = ENC_TEST_FLASH_ID,
        .fa_off = 0x00008000,
        .fa_size = 16 * 1024
    },
    [3] = {
        .fa_id = ENC_TEST_FLASH_ID,
        .fa_off = 0x0000c000,
        .fa_size = 16 * 1024
    }
};

TEST_SUITE(enc_flash_test_all)
{
    enc_flash_test_hal();
    enc_flash_test_flash_map();
    enc_flash_test_fcb();
}

int
main(int argc, char **argv)
{
    enc_flash_test_all();
    return tu_any_failed;
}
