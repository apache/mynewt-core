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
#include "fcb_test.h"

TEST_CASE_SELF(fcb_test_len)
{
    uint8_t buf[3];
    uint16_t len;
    uint16_t len2;
    int rc;
    int rc2;

    fcb_tc_pretest(2);

    for (len = 0; len < FCB_MAX_LEN; len++) {
        rc = fcb_put_len(buf, len);
        TEST_ASSERT(rc == 1 || rc == 2);

        rc2 = fcb_get_len(buf, &len2);
        TEST_ASSERT(rc2 == rc);

        TEST_ASSERT(len == len2);
    }
}
