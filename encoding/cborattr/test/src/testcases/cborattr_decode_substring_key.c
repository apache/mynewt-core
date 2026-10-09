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
 * Where we collect cbor data.
 */
static uint8_t test_cbor_buf[1024];
static int test_cbor_len;

/*
 * CBOR encoder data structures.
 */
static CborError test_cbor_wr(void *, const void *, size_t, CborEncoderAppendType);
static CborEncoder test_encoder;
static CborError
test_cbor_wr(void *token, const void *data, size_t len, CborEncoderAppendType append)
{
    memcpy(test_cbor_buf + test_cbor_len, data, len);
    test_cbor_len += len;

    assert(test_cbor_len < sizeof(test_cbor_buf));
    return CborNoError;
}

static void
test_encode_substring_key(void)
{
    CborEncoder data;

    cbor_encoder_init_writer(&test_encoder, test_cbor_wr, NULL);

    /*
     * { "a": "A", "aa": "AA", "aaa" : "AAA" }
     */
    cbor_encoder_create_map(&test_encoder, &data, CborIndefiniteLength);

    cbor_encode_text_stringz(&data, "a");
    cbor_encode_text_stringz(&data, "A");
    cbor_encode_text_stringz(&data, "aa");
    cbor_encode_text_stringz(&data, "AA");
    cbor_encode_text_stringz(&data, "aaa");
    cbor_encode_text_stringz(&data, "AAA");

    cbor_encoder_close_container(&test_encoder, &data);
}

/*
 * substring key
 */
TEST_CASE(test_cborattr_decode_substring_key)
{
    int rc;
    char test_str_1a[4] = { '\0' };
    char test_str_2a[4] = { '\0' };
    char test_str_3a[4] = { '\0' };
    struct cbor_attr_t test_attrs[] = {
        [0] = { .attribute = "aaa",
               .type = CborAttrTextStringType,
               .addr.string = test_str_3a,
               .len = sizeof(test_str_3a),
               .nodefault = true },
        [1] = { .attribute = "aa",
               .type = CborAttrTextStringType,
               .addr.string = test_str_2a,
               .len = sizeof(test_str_2a),
               .nodefault = true },
        [2] = { .attribute = "a",
               .type = CborAttrTextStringType,
               .addr.string = test_str_1a,
               .len = sizeof(test_str_1a),
               .nodefault = true },
        [3] = { .attribute = NULL }
    };

    test_encode_substring_key();

    rc = cbor_read_flat_attrs(test_cbor_buf, test_cbor_len, test_attrs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(!strcmp(test_str_1a, "A"));
    TEST_ASSERT(!strcmp(test_str_2a, "AA"));
    TEST_ASSERT(!strcmp(test_str_3a, "AAA"));
}
