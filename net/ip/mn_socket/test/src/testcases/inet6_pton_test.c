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

TEST_CASE(inet6_pton_test)
{
    uint8_t addr_bytes[32];
    int rc;
    int i;

    struct test_vec {
        const char *str;
        struct mn_in6_addr addr;
    };

    static const struct test_vec ok_vec[] = {
        {
            "::",
            { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }
        },
        {
            "1::",
            { { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }
        },
        {
            "1::f",
            { { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xf } }
        },
        {
            "1234:5678::abcd:ef01",
            { { 0x12, 0x34, 0x56, 0x78, 0, 0, 0, 0,
                0, 0, 0, 0, 0xab, 0xcd, 0xef, 0x01 } }
        },
        {
            "5315:afaa:985e:72ca:9889:1632:8775:bbba",
            { { 0x53, 0x15, 0xaf, 0xaa, 0x98, 0x5e, 0x72, 0xca,
                0x98, 0x89, 0x16, 0x32, 0x87, 0x75, 0xbb, 0xba } }
        },
        {
            "::1:2:3:4",
            { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2, 0, 3, 0, 4 } }
        },
        {
            "::1:2:3",
            { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2, 0, 3 } }
        },
    };

    static const struct test_vec invalid_vec[] = {
        { "" },
        { ":" },
        { "1::2::3" },
        { "11111:43:a:b:c:d:e:f" },
        { "g::a" },
    };


    for (i = 0; i < sizeof(ok_vec) / sizeof(ok_vec[0]); i++) {
        memset(addr_bytes, 0xa5, sizeof(addr_bytes));
        rc = mn_inet_pton(MN_PF_INET6, ok_vec[i].str, addr_bytes);
        TEST_ASSERT_FATAL(rc == 1,
                          "inet_pton(\"%s\")", ok_vec[i].str);
        TEST_ASSERT(memcmp(addr_bytes, &ok_vec[i].addr, 16) == 0,
                    "inet_pton(\"%s\")", ok_vec[i].str);
        TEST_ASSERT(addr_bytes[16] == 0xa5,
                    "inet_pton(\"%s\")", ok_vec[i].str);
    }

    for (i = 0; i < sizeof(invalid_vec) / sizeof(invalid_vec[0]); i++) {
        rc = mn_inet_pton(MN_PF_INET6, invalid_vec[i].str, addr_bytes);
        TEST_ASSERT(rc == 0,
                    "inet_pton(\"%s\")", invalid_vec[i].str);
    }
}
