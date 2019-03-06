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

#ifndef __ENC_FLASH_TEST_H_
#define __ENC_FLASH_TEST_H_

#include <testutil/testutil.h>

#define ENC_TEST_FLASH_AREA_CNT                                         \
    (sizeof(enc_test_flash_areas) / sizeof(enc_test_flash_areas[0]))

TEST_CASE_DECL(enc_flash_test_hal)
TEST_CASE_DECL(enc_flash_test_flash_map)
TEST_CASE_DECL(enc_flash_test_fcb)

extern struct flash_area enc_test_flash_areas[4];

#endif
