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

/*
 * Values of unexpected type must be rejected with an error instead of
 * being passed to tinycbor getters, which assert on type mismatch.
 */
TEST_CASE_SELF(test_cborattr_decode_type_mismatch)
{
    /* {"a": -1} */
    static const uint8_t neg[] = { 0xa1, 0x61, 'a', 0x20 };
    /* {"a": []} */
    static const uint8_t empty[] = { 0xa1, 0x61, 'a', 0x80 };
    /* {"a": [-1]} */
    static const uint8_t neg_arr[] = { 0xa1, 0x61, 'a', 0x81, 0x20 };
    /* {"a": [1]} */
    static const uint8_t int_arr[] = { 0xa1, 0x61, 'a', 0x81, 0x01 };
    /* {"a": [1, "x"]} */
    static const uint8_t mixed_arr[] = { 0xa1, 0x61, 'a', 0x82, 0x01, 0x61, 'x' };
    /* {"a": ["x"]} */
    static const uint8_t str_arr[] = { 0xa1, 0x61, 'a', 0x81, 0x61, 'x' };
#if MYNEWT_VAL(FLOAT_USER)
    /* {"a": [1.5f]} */
    static const uint8_t float_arr[] = { 0xa1, 0x61, 'a',  0x81, 0xfa,
                                         0x3f, 0xc0, 0x00, 0x00 };
    /* {"a": [1.5]} */
    static const uint8_t double_arr[] = { 0xa1, 0x61, 'a',  0x81, 0xfb,
                                          0x3f, 0xf8, 0x00, 0x00, 0x00,
                                          0x00, 0x00, 0x00 };
    double reals[2];
#endif
    long long unsigned int uval;
    long long unsigned int uints[2];
    long long int ints[2];
    bool bools[2];
    char *ptrs[2];
    char store[8];
    int cnt;
    int rc;

    struct cbor_attr_t uint_attrs[] = {
        {
         .attribute = "a",
         .type = CborAttrUnsignedIntegerType,
         .addr.uinteger = &uval,
         },
        { 0 },
    };
    struct cbor_attr_t arr_attrs[] = {
        {
         .attribute = "a",
         .type = CborAttrArrayType,
         .addr.array.count = &cnt,
         .addr.array.maxlen = 2,
         },
        { 0 },
    };
    struct cbor_array_t *arr = &arr_attrs[0].addr.array;

    /* Negative value for unsigned integer */
    rc = cbor_read_flat_attrs(neg, sizeof(neg), uint_attrs);
    TEST_ASSERT(rc == CborErrorIllegalType);

    /* Empty arrays of all element types */
    arr->element_type = CborAttrIntegerType;
    arr->arr.integers.store = ints;
    cnt = -1;
    rc = cbor_read_flat_attrs(empty, sizeof(empty), arr_attrs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(cnt == 0);

    arr->element_type = CborAttrBooleanType;
    arr->arr.booleans.store = bools;
    cnt = -1;
    rc = cbor_read_flat_attrs(empty, sizeof(empty), arr_attrs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(cnt == 0);

    arr->element_type = CborAttrTextStringType;
    arr->arr.strings.ptrs = ptrs;
    arr->arr.strings.store = store;
    arr->arr.strings.storelen = sizeof(store);
    cnt = -1;
    rc = cbor_read_flat_attrs(empty, sizeof(empty), arr_attrs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(cnt == 0);

    /* Wrong element type for text string array */
    rc = cbor_read_flat_attrs(int_arr, sizeof(int_arr), arr_attrs);
    TEST_ASSERT(rc != 0);
    TEST_ASSERT(cnt == 0);

    /* Wrong element type for boolean array */
    arr->element_type = CborAttrBooleanType;
    arr->arr.booleans.store = bools;
    rc = cbor_read_flat_attrs(int_arr, sizeof(int_arr), arr_attrs);
    TEST_ASSERT(rc != 0);
    TEST_ASSERT(cnt == 0);

    /* Wrong element type for integer array */
    arr->element_type = CborAttrIntegerType;
    arr->arr.integers.store = ints;
    rc = cbor_read_flat_attrs(str_arr, sizeof(str_arr), arr_attrs);
    TEST_ASSERT(rc != 0);
    TEST_ASSERT(cnt == 0);

    /* Wrong type of second element, first one is decoded */
    rc = cbor_read_flat_attrs(mixed_arr, sizeof(mixed_arr), arr_attrs);
    TEST_ASSERT(rc != 0);
    TEST_ASSERT(cnt == 1);
    TEST_ASSERT(ints[0] == 1);

    /* Negative value in unsigned integer array */
    arr->element_type = CborAttrUnsignedIntegerType;
    arr->arr.uintegers.store = uints;
    rc = cbor_read_flat_attrs(neg_arr, sizeof(neg_arr), arr_attrs);
    TEST_ASSERT(rc != 0);
    TEST_ASSERT(cnt == 0);

    rc = cbor_read_flat_attrs(int_arr, sizeof(int_arr), arr_attrs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(cnt == 1);
    TEST_ASSERT(uints[0] == 1);

#if MYNEWT_VAL(FLOAT_USER)
    /* Float and double arrays */
    arr->element_type = CborAttrFloatType;
    arr->arr.reals.store = reals;
    rc = cbor_read_flat_attrs(float_arr, sizeof(float_arr), arr_attrs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(cnt == 1);
    TEST_ASSERT(reals[0] == 1.5);

    rc = cbor_read_flat_attrs(double_arr, sizeof(double_arr), arr_attrs);
    TEST_ASSERT(rc != 0);
    TEST_ASSERT(cnt == 0);

    arr->element_type = CborAttrDoubleType;
    rc = cbor_read_flat_attrs(double_arr, sizeof(double_arr), arr_attrs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(cnt == 1);
    TEST_ASSERT(reals[0] == 1.5);

    rc = cbor_read_flat_attrs(float_arr, sizeof(float_arr), arr_attrs);
    TEST_ASSERT(rc != 0);
    TEST_ASSERT(cnt == 0);
#endif
}
