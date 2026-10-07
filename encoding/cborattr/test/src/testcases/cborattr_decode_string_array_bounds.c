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

static uint8_t test_arr_buf[64];
static int test_arr_len;

static int
test_arr_wr(struct cbor_encoder_writer *cew, const char *data, int len)
{
    memcpy(test_arr_buf + test_arr_len, data, len);
    test_arr_len += len;
    assert(test_arr_len < sizeof(test_arr_buf));
    return 0;
}

/* Encodes {"a": [strs...]} */
static void
test_arr_encode(const char **strs, int cnt)
{
    struct cbor_encoder_writer writer = { .write = test_arr_wr };
    CborEncoder enc;
    CborEncoder map;
    CborEncoder arr;
    int i;

    test_arr_len = 0;
    cbor_encoder_init(&enc, &writer, 0);
    cbor_encoder_create_map(&enc, &map, CborIndefiniteLength);
    cbor_encode_text_stringz(&map, "a");
    cbor_encoder_create_array(&map, &arr, CborIndefiniteLength);
    for (i = 0; i < cnt; i++) {
        cbor_encode_text_stringz(&arr, strs[i]);
    }
    cbor_encoder_close_container(&map, &arr);
    cbor_encoder_close_container(&enc, &map);
}

TEST_CASE_SELF(test_cborattr_decode_string_array_bounds)
{
    const char *fit[] = { "ab", "cd" };
    const char *no_fit[] = { "abcdefgh", "zzzzzzzzzzzz", "zzzzzzzzzzzz" };
    struct {
        char store[8];
        char guard[24];
    } buf;
    char *ptrs[3];
    int cnt;
    int rc;
    int i;

    const struct cbor_attr_t attrs[] = {
        {
            .attribute = "a",
            .type = CborAttrArrayType,
            .addr.array.element_type = CborAttrTextStringType,
            .addr.array.arr.strings.ptrs = ptrs,
            .addr.array.arr.strings.store = buf.store,
            .addr.array.arr.strings.storelen = sizeof(buf.store),
            .addr.array.count = &cnt,
            .addr.array.maxlen = 3,
        },
        { 0 },
    };

    /* Strings fitting in store are decoded. */
    test_arr_encode(fit, 2);
    rc = cbor_read_flat_attrs(test_arr_buf, test_arr_len, attrs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(cnt == 2);
    TEST_ASSERT(strcmp(ptrs[0], "ab") == 0);
    TEST_ASSERT(strcmp(ptrs[1], "cd") == 0);

    /* String not fitting in store is rejected and nothing is written past
     * the store.
     */
    memset(buf.guard, 0x5a, sizeof(buf.guard));
    test_arr_encode(no_fit, 3);
    rc = cbor_read_flat_attrs(test_arr_buf, test_arr_len, attrs);
    TEST_ASSERT(rc != 0);
    TEST_ASSERT(cnt == 0);
    for (i = 0; i < sizeof(buf.guard); i++) {
        TEST_ASSERT_FATAL(buf.guard[i] == 0x5a);
    }
}
