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

TEST_CASE(inet_ntop_test)
{
    const char *rstr;
    char addr[48];
    struct test_vec {
        char *str;
        uint8_t cmp[4];
    };
    struct test_vec ok_vec[] = {
        { "1.1.1.1", { 1, 1, 1, 1 } },
        { "1.2.3.4", { 1, 2, 3, 4 } },
        { "255.1.255.255", { 255, 1, 255, 255 } },
        { "1.2.5.6", { 1, 2, 5, 6 } }
    };
    int i;

    for (i = 0; i < sizeof(ok_vec) / sizeof(ok_vec[0]); i++) {
        memset(addr, 0xa5, sizeof(addr));
        rstr = mn_inet_ntop(MN_PF_INET, ok_vec[i].cmp, addr, sizeof(addr));
        TEST_ASSERT(rstr);
        TEST_ASSERT(!strcmp(ok_vec[i].str, addr));
    }
    rstr = mn_inet_ntop(MN_PF_INET, ok_vec[0].cmp, addr, 1);
    TEST_ASSERT(rstr == NULL);

    /* does not have space to null terminate */
    rstr = mn_inet_ntop(MN_PF_INET, ok_vec[0].cmp, addr, 7);
    TEST_ASSERT(rstr == NULL);
}
