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

#include "mn_sock_test.h"

TEST_CASE(inet_pton_test)
{
    int rc;
    uint8_t addr[8];
    struct test_vec {
        char *str;
        uint8_t cmp[4];
    };
    struct test_vec ok_vec[] = {
        { "1.1.1.1", { 1, 1, 1, 1 } },
        { "1.2.3.4", { 1, 2, 3, 4 } },
        { "010.001.255.255", { 10, 1, 255, 255 } },
        { "001.002.005.006", { 1, 2, 5, 6 } }
    };
    struct test_vec invalid_vec[] = {
        { "a.b.c.d" },
        { "1a.b3.4.2" },
        { "1.3.4.2a" },
        { "1111.3.4.2" },
        { "3.256.1.0" },
    };
    int i;

    for (i = 0; i < sizeof(ok_vec) / sizeof(ok_vec[0]); i++) {
        memset(addr, 0xa5, sizeof(addr));
        rc = mn_inet_pton(MN_PF_INET, ok_vec[i].str, addr);
        TEST_ASSERT(rc == 1);
        TEST_ASSERT(!memcmp(ok_vec[i].cmp, addr, sizeof(uint32_t)));
        TEST_ASSERT(addr[5] == 0xa5);
    }
    for (i = 0; i < sizeof(invalid_vec) / sizeof(invalid_vec[0]); i++) {
        rc = mn_inet_pton(MN_PF_INET, invalid_vec[i].str, addr);
        TEST_ASSERT(rc == 0);
    }
}
