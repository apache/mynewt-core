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

static uint8_t test_dflt_buf[32];
static int test_dflt_len;

static CborError
test_dflt_wr(void *token, const void *data, size_t len, CborEncoderAppendType append)
{
    memcpy(test_dflt_buf + test_dflt_len, data, len);
    test_dflt_len += len;
    assert(test_dflt_len < sizeof(test_dflt_buf));
    return CborNoError;
}

/* Encodes {key: 1} */
static void
test_dflt_encode(const char *key)
{
    CborEncoder enc;
    CborEncoder map;

    test_dflt_len = 0;
    cbor_encoder_init_writer(&enc, test_dflt_wr, NULL);
    cbor_encoder_create_map(&enc, &map, CborIndefiniteLength);
    cbor_encode_text_stringz(&map, key);
    cbor_encode_int(&map, 1);
    cbor_encoder_close_container(&enc, &map);
}

TEST_CASE_SELF(test_cborattr_decode_array_default)
{
    char *ptrs[2];
    char store[16];
    int cnt;
    int rc;

    struct cbor_attr_t attrs[] = {
        {
         .attribute = "a",
         .type = CborAttrArrayType,
         .addr.array.element_type = CborAttrTextStringType,
         .addr.array.arr.strings.ptrs = ptrs,
         .addr.array.arr.strings.store = store,
         .addr.array.arr.strings.storelen = sizeof(store),
         .addr.array.count = &cnt,
         .addr.array.maxlen = 2,
         },
        { 0 },
    };

    /* Array attribute missing */
    test_dflt_encode("b");
    cnt = 1234;
    rc = cbor_read_flat_attrs(test_dflt_buf, test_dflt_len, attrs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(cnt == 0);

    /* Array attribute present, but not an array */
    test_dflt_encode("a");
    cnt = 1234;
    rc = cbor_read_flat_attrs(test_dflt_buf, test_dflt_len, attrs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(cnt == 0);

    /* No default requested, count left untouched */
    attrs[0].nodefault = true;
    test_dflt_encode("b");
    cnt = 1234;
    rc = cbor_read_flat_attrs(test_dflt_buf, test_dflt_len, attrs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(cnt == 1234);
}
