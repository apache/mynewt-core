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

static uint8_t test_str_buf[64];
static int test_str_len;

static int
test_str_wr(struct cbor_encoder_writer *cew, const char *data, int len)
{
    memcpy(test_str_buf + test_str_len, data, len);
    test_str_len += len;
    assert(test_str_len < sizeof(test_str_buf));
    return 0;
}

/* Encodes {"s": <text>, "b": <bytes>, "i": 1}, omitting NULL members. */
static void
test_str_encode(const char *s, const uint8_t *b, size_t b_len)
{
    struct cbor_encoder_writer writer = { .write = test_str_wr };
    CborEncoder enc;
    CborEncoder map;

    test_str_len = 0;
    cbor_encoder_init(&enc, &writer, 0);
    cbor_encoder_create_map(&enc, &map, CborIndefiniteLength);
    if (s) {
        cbor_encode_text_stringz(&map, "s");
        cbor_encode_text_stringz(&map, s);
    }
    if (b) {
        cbor_encode_text_stringz(&map, "b");
        cbor_encode_byte_string(&map, b, b_len);
    }
    cbor_encode_text_stringz(&map, "i");
    cbor_encode_uint(&map, 1);
    cbor_encoder_close_container(&enc, &map);
}

TEST_CASE_SELF(test_cborattr_decode_string_bounds)
{
    const uint8_t bytes[8] = { 0 };
    uint8_t b[4];
    size_t b_len;
    char s[4];
    long long int i;
    int rc;

    const struct cbor_attr_t attrs[] = {
        {
            .attribute = "s",
            .type = CborAttrTextStringType,
            .addr.string = s,
            .len = sizeof(s),
        },
        {
            .attribute = "b",
            .type = CborAttrByteStringType,
            .addr.bytestring.data = b,
            .addr.bytestring.len = &b_len,
            .len = sizeof(b),
        },
        {
            .attribute = "i",
            .type = CborAttrIntegerType,
            .addr.integer = &i,
        },
        { 0 },
    };

    /* String shorter than buffer is decoded and terminated. */
    memset(s, 'x', sizeof(s));
    test_str_encode("abc", bytes, 4);
    rc = cbor_read_flat_attrs(test_str_buf, test_str_len, attrs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(strcmp(s, "abc") == 0);
    TEST_ASSERT(b_len == 4);
    TEST_ASSERT(i == 1);

    /* String filling whole buffer is rejected, buffer is left terminated. */
    memset(s, 'x', sizeof(s));
    test_str_encode("abcd", NULL, 0);
    rc = cbor_read_flat_attrs(test_str_buf, test_str_len, attrs);
    TEST_ASSERT(rc != 0);
    TEST_ASSERT(s[0] == '\0');

    /* Too long byte string is rejected and reports zero length. */
    test_str_encode(NULL, bytes, sizeof(bytes));
    rc = cbor_read_flat_attrs(test_str_buf, test_str_len, attrs);
    TEST_ASSERT(rc != 0);
    TEST_ASSERT(b_len == 0);

    /* Omitted string and byte string are set to empty. */
    memset(s, 'x', sizeof(s));
    b_len = 1234;
    test_str_encode(NULL, NULL, 0);
    rc = cbor_read_flat_attrs(test_str_buf, test_str_len, attrs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(s[0] == '\0');
    TEST_ASSERT(b_len == 0);
}
