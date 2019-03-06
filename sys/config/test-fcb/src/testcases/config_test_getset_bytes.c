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
#include "conf_test_fcb.h"

TEST_CASE_SELF(config_test_getset_bytes)
{
    char orig[32];
    char bytes[32];
    char str[48];
    char *ret;
    int j, i;
    int tmp;
    int rc;

    for (j = 1; j < sizeof(orig); j++) {
        for (i = 0; i < j; i++) {
            orig[i] = i + j + 1;
        }
        ret = conf_str_from_bytes(orig, j, str, sizeof(str));
        TEST_ASSERT(ret);
        tmp = strlen(str);
        TEST_ASSERT(tmp < sizeof(str));

        memset(bytes, 0, sizeof(bytes));
        tmp = sizeof(bytes);

        tmp = sizeof(bytes);
        rc = conf_bytes_from_str(str, bytes, &tmp);
        TEST_ASSERT(rc == 0);
        TEST_ASSERT(tmp == j);
        TEST_ASSERT(!memcmp(orig, bytes, j));
    }
}
