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

#include "test_cborattr.h"

#define TEST_NESTING_DEEP   200

static uint8_t test_nesting_buf[TEST_NESTING_DEEP + 32];

/*
 * Encodes {"a": 1, "x": <value>} or {"a": [1, <value>]} where value is an
 * integer nested in specified number of single element arrays.
 */
static int
test_nesting_encode(int depth, bool in_array)
{
    int len = 0;
    int i;

    test_nesting_buf[len++] = in_array ? 0xa1 : 0xa2;
    test_nesting_buf[len++] = 0x61;
    test_nesting_buf[len++] = 'a';
    if (in_array) {
        test_nesting_buf[len++] = 0x82;
        test_nesting_buf[len++] = 0x01;
    } else {
        test_nesting_buf[len++] = 0x01;
        test_nesting_buf[len++] = 0x61;
        test_nesting_buf[len++] = 'x';
    }
    for (i = 0; i < depth; i++) {
        test_nesting_buf[len++] = 0x81;
    }
    test_nesting_buf[len++] = 0x00;

    return len;
}

TEST_CASE_SELF(test_cborattr_decode_nesting)
{
    long long int a;
    long long int arr[1];
    int arr_cnt;
    int len;
    int rc;

    const struct cbor_attr_t attrs[] = {
        {
            .attribute = "a",
            .type = CborAttrIntegerType,
            .addr.integer = &a,
        },
        { 0 },
    };
    const struct cbor_attr_t arr_attrs[] = {
        {
            .attribute = "a",
            .type = CborAttrArrayType,
            .addr.array.element_type = CborAttrIntegerType,
            .addr.array.arr.integers.store = arr,
            .addr.array.count = &arr_cnt,
            .addr.array.maxlen = 1,
        },
        { 0 },
    };

    /* Unknown attribute with nested value is skipped. */
    len = test_nesting_encode(4, false);
    a = 0;
    rc = cbor_read_flat_attrs(test_nesting_buf, len, attrs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(a == 1);

    /* Value nested deeper than allowed is rejected. */
    len = test_nesting_encode(TEST_NESTING_DEEP, false);
    rc = cbor_read_flat_attrs(test_nesting_buf, len, attrs);
    TEST_ASSERT(rc != 0);

    /* Excess array element nested deeper than allowed is rejected. */
    len = test_nesting_encode(TEST_NESTING_DEEP, true);
    rc = cbor_read_flat_attrs(test_nesting_buf, len, arr_attrs);
    TEST_ASSERT(rc != 0);
}
