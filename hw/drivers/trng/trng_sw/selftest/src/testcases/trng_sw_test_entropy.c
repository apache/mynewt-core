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

#include <os/mynewt.h>

#include <trng_sw/trng_sw.h>

#include "trng_sw_test.h"

TEST_CASE_SELF(trng_sw_test_add_entropy)
{
    struct trng_sw_dev *tsd;
    int rc;
    int i;
    uint8_t somedata[64];

    tsd = (struct trng_sw_dev *)os_dev_lookup("trng");
    TEST_ASSERT_FATAL(tsd != NULL);

    memset(somedata, 0xa5, sizeof(somedata));
    for (i = 0; i < sizeof(somedata); i++) {
        rc = trng_sw_dev_add_entropy(tsd, somedata, i);
        TEST_ASSERT(rc == 0);
    }
}
