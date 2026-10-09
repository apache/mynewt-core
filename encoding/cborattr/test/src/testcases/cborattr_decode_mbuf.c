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

#include "os/mynewt.h"
#include "test_cborattr.h"

/* Small mbuf blocks so that strings span multiple mbufs */
#define TEST_MBUF_BUF_SIZE 20
#define TEST_MBUF_BLOCK_SIZE                                                  \
    (TEST_MBUF_BUF_SIZE + sizeof(struct os_mbuf) + sizeof(struct os_mbuf_pkthdr))
#define TEST_MBUF_COUNT    64

/* Offset of CBOR data in mbuf, e.g. protocol header */
#define TEST_MBUF_DATA_OFF 5

static os_membuf_t test_mbuf_mem[OS_MEMPOOL_SIZE(TEST_MBUF_COUNT, TEST_MBUF_BLOCK_SIZE)];
static struct os_mempool test_mbuf_mempool;
static struct os_mbuf_pool test_mbuf_pool;

static uint8_t test_buf[512];
static size_t test_len;

static const char test_name[] = "name long enough to span multiple mbufs";
static const char test_skip[] = "value of attribute that is not decoded but "
                                "must be skipped";

static void
test_encode_req(void)
{
    CborEncoder enc, map, str;
    uint8_t data[200];
    int i;

    for (i = 0; i < sizeof(data); i++) {
        data[i] = i;
    }

    cbor_encoder_init(&enc, test_buf, sizeof(test_buf), 0);
    cbor_encoder_create_map(&enc, &map, CborIndefiniteLength);
    cbor_encode_text_stringz(&map, "skip");
    cbor_encode_text_stringz(&map, test_skip);
    cbor_encode_text_stringz(&map, "off");
    cbor_encode_uint(&map, 1234);
    cbor_encode_text_stringz(&map, "data");
    cbor_encode_byte_string(&map, data, sizeof(data));
    cbor_encode_text_stringz(&map, "name");
    cbor_encode_text_stringz(&map, test_name);
    cbor_encode_text_stringz(&map, "chunked");
    cbor_encoder_create_indef_text_string(&map, &str);
    cbor_encode_text_stringz(&str, "first chunk, ");
    cbor_encode_text_stringz(&str, "second chunk");
    cbor_encoder_close_container(&map, &str);
    TEST_ASSERT_FATAL(cbor_encoder_close_container(&enc, &map) == 0);
    test_len = cbor_encoder_get_buffer_size(&enc, test_buf);
}

static struct os_mbuf *
test_mbuf_req(void)
{
    static const uint8_t hdr[TEST_MBUF_DATA_OFF];
    struct os_mbuf *om;

    om = os_mbuf_get_pkthdr(&test_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om != NULL);
    TEST_ASSERT_FATAL(os_mbuf_append(om, hdr, sizeof(hdr)) == 0);
    TEST_ASSERT_FATAL(os_mbuf_append(om, test_buf, test_len) == 0);

    return om;
}

TEST_CASE_SELF(test_cborattr_decode_mbuf)
{
    long long unsigned int off;
    uint8_t data[200];
    size_t data_len;
    char name[64];
    char chunked[32];
    struct os_mbuf *om;
    int rc;
    int i;

    const struct cbor_attr_t attrs[] = {
        {
         .attribute = "off",
         .type = CborAttrUnsignedIntegerType,
         .addr.uinteger = &off,
         },
        {
         .attribute = "data",
         .type = CborAttrByteStringType,
         .addr.bytestring.data = data,
         .addr.bytestring.len = &data_len,
         .len = sizeof(data),
         },
        {
         .attribute = "name",
         .type = CborAttrTextStringType,
         .addr.string = name,
         .len = sizeof(name),
         },
        {
         .attribute = "chunked",
         .type = CborAttrTextStringType,
         .addr.string = chunked,
         .len = sizeof(chunked),
         },
        { 0 },
    };

    rc = os_mempool_init(&test_mbuf_mempool, TEST_MBUF_COUNT,
                         TEST_MBUF_BLOCK_SIZE, test_mbuf_mem, "test_attr");
    TEST_ASSERT_FATAL(rc == 0);
    rc = os_mbuf_pool_init(&test_mbuf_pool, &test_mbuf_mempool,
                           TEST_MBUF_BLOCK_SIZE, TEST_MBUF_COUNT);
    TEST_ASSERT_FATAL(rc == 0);

    test_encode_req();
    om = test_mbuf_req();

    rc = cbor_read_mbuf_attrs(om, TEST_MBUF_DATA_OFF, test_len, attrs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(off == 1234);
    TEST_ASSERT(data_len == sizeof(data));
    for (i = 0; i < sizeof(data); i++) {
        TEST_ASSERT(data[i] == i);
    }
    TEST_ASSERT(strcmp(name, test_name) == 0);
    TEST_ASSERT(strcmp(chunked, "first chunk, second chunk") == 0);

    /* Same result when decoding from flat buffer */
    memset(data, 0, sizeof(data));
    memset(name, 0, sizeof(name));
    rc = cbor_read_flat_attrs(test_buf, test_len, attrs);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(off == 1234);
    TEST_ASSERT(data_len == sizeof(data));
    TEST_ASSERT(data[sizeof(data) - 1] == sizeof(data) - 1);
    TEST_ASSERT(strcmp(name, test_name) == 0);

    os_mbuf_free_chain(om);
    TEST_ASSERT(test_mbuf_mempool.mp_num_free == TEST_MBUF_COUNT);
}
