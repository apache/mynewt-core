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

#include <string.h>

#include <oic/oc_uuid.h>
#include "test_oic.h"

void
test_uuid(void)
{
    static const char valid_uuid[] = "00112233-4455-6677-8899-aabbccddeeff";
    static const uint8_t expected[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
                                        0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb,
                                        0xcc, 0xdd, 0xee, 0xff };
    char oversized_uuid[35];
    struct {
        oc_uuid_t uuid;
        uint8_t guard;
    } result;

    memset(&result, 0xa5, sizeof(result));
    oc_str_to_uuid(valid_uuid, &result.uuid);
    TEST_ASSERT(memcmp(result.uuid.id, expected, sizeof(expected)) == 0);

    memset(oversized_uuid, 'a', sizeof(oversized_uuid) - 1);
    oversized_uuid[sizeof(oversized_uuid) - 1] = '\0';
    result.guard = 0x5a;
    oc_str_to_uuid(oversized_uuid, &result.uuid);
    TEST_ASSERT(result.guard == 0x5a);
}
