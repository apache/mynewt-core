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
#include <trng/trng.h>

#include "trng_sw_test.h"

TEST_CASE_SELF(trng_sw_test_read)
{
    struct trng_dev *dev;
    int rc;
    uint32_t val1, val2, val3;
    int i;
    uint8_t data[32];

    dev = (struct trng_dev *)os_dev_lookup("trng");
    TEST_ASSERT_FATAL(dev != NULL);

    val1 = trng_get_u32(dev);
    val2 = trng_get_u32(dev);
    TEST_ASSERT(val1 != val2);

    rc = trng_read(dev, &val3, sizeof(val3));
    TEST_ASSERT(rc == sizeof(val3));
    TEST_ASSERT(val1 != val2);
    TEST_ASSERT(val1 != val3);
    TEST_ASSERT(val2 != val3);

    rc = trng_read(dev, &val2, sizeof(val2));
    TEST_ASSERT(rc == sizeof(val2));
    TEST_ASSERT(val3 != val2);

    for (i = 1; i < sizeof(data); i++) {
        rc = trng_read(dev, data, i);
        TEST_ASSERT(rc == i);
    }
}
