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
#include "encoding_test_priv.h"

TEST_CASE_SELF(hex2str)
{
    int i;
    char *ret;
    char cmp_data[8];

    struct {
        char *in;
        int inlen;
        char *out;
        int outlen;
    } test_data[] = {
        [0] = {
            .in = "\x01",
            .inlen = 1,
            .out = "01",
            .outlen = 2,
        },
        [1] = {
            .in = "\xaf\xf2",
            .inlen = 2,
            .out = "aff2",
            .outlen = 4,
        }
    };

    for (i = 0; i < sizeof(test_data) / sizeof(test_data[0]); i++) {
        ret = hex_format(test_data[i].in, test_data[i].inlen,
          cmp_data, sizeof(cmp_data));
        TEST_ASSERT(ret == cmp_data);
        TEST_ASSERT(strlen(cmp_data) == test_data[i].outlen);
        TEST_ASSERT(!strcmp(test_data[i].out, cmp_data));
    }

    /*
     * Test not enough space. Must have space for '\0' at the end.
     */
    ret = hex_format("\x01\x02", 2, cmp_data, 1);
    TEST_ASSERT(ret == NULL);

    ret = hex_format("\x01\x02", 2, cmp_data, 2);
    TEST_ASSERT(ret == NULL);
}
