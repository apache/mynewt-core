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

#include <string.h>
#include "os/mynewt.h"
#include "sysinit/sysinit.h"
#include "testutil/testutil.h"
#include "tinycbor/cbor.h"
#include "tinycbor/cbor_cnt_writer.h"
#include "tinycbor/cbor_mbuf_reader.h"
#include "tinycbor/cbor_mbuf_writer.h"

/* Small mbuf blocks so that encoded data spans many mbufs */
#define TEST_MBUF_BUF_SIZE 24
#define TEST_MBUF_BLOCK_SIZE                                                  \
    (TEST_MBUF_BUF_SIZE + sizeof(struct os_mbuf) + sizeof(struct os_mbuf_pkthdr))
#define TEST_MBUF_COUNT 128

static os_membuf_t test_mbuf_mem[OS_MEMPOOL_SIZE(TEST_MBUF_COUNT, TEST_MBUF_BLOCK_SIZE)];
static struct os_mempool test_mbuf_mempool;
static struct os_mbuf_pool test_mbuf_pool;

static uint8_t test_flat[512];
static size_t test_flat_len;

static const char test_text[] =
    "Text string long enough to span multiple mbufs used in this test";

static void
test_mbuf_init(void)
{
    int rc;

    rc = os_mempool_init(&test_mbuf_mempool, TEST_MBUF_COUNT,
                         TEST_MBUF_BLOCK_SIZE, test_mbuf_mem, "test_cbor");
    TEST_ASSERT_FATAL(rc == 0);

    rc = os_mbuf_pool_init(&test_mbuf_pool, &test_mbuf_mempool,
                           TEST_MBUF_BLOCK_SIZE, TEST_MBUF_COUNT);
    TEST_ASSERT_FATAL(rc == 0);
}

static struct os_mbuf *
test_mbuf_get(void)
{
    struct os_mbuf *om;

    om = os_mbuf_get_pkthdr(&test_mbuf_pool, 0);
    TEST_ASSERT_FATAL(om != NULL);

    return om;
}

static void
test_mbuf_check_free(void)
{
    TEST_ASSERT(test_mbuf_mempool.mp_num_free == TEST_MBUF_COUNT);
}

/* Encodes test document covering all major types and header sizes */
static void
test_encode_doc(CborEncoder *enc)
{
    CborEncoder map, arr, sub, str;
    uint8_t bytes[64];
    int i;

    for (i = 0; i < sizeof(bytes); i++) {
        bytes[i] = i;
    }

    TEST_ASSERT_FATAL(cbor_encoder_create_map(enc, &map, CborIndefiniteLength) == 0);
    cbor_encode_text_stringz(&map, "u8");
    cbor_encode_uint(&map, 23);
    cbor_encode_text_stringz(&map, "u16");
    cbor_encode_uint(&map, 1000);
    cbor_encode_text_stringz(&map, "u32");
    cbor_encode_uint(&map, 100000);
    cbor_encode_text_stringz(&map, "u64");
    cbor_encode_uint(&map, 0x100000000ULL);
    cbor_encode_text_stringz(&map, "neg");
    cbor_encode_int(&map, -500);
    cbor_encode_text_stringz(&map, "text");
    cbor_encode_text_stringz(&map, test_text);
    cbor_encode_text_stringz(&map, "bytes");
    cbor_encode_byte_string(&map, bytes, sizeof(bytes));

    cbor_encode_text_stringz(&map, "arr");
    cbor_encoder_create_array(&map, &arr, 5);
    cbor_encode_uint(&arr, 1);
    cbor_encode_text_stringz(&arr, "x");
    cbor_encode_byte_string(&arr, bytes, 2);
    cbor_encoder_create_array(&arr, &sub, 3);
    cbor_encode_boolean(&sub, true);
    cbor_encode_boolean(&sub, false);
    cbor_encode_null(&sub);
    cbor_encoder_close_container(&arr, &sub);
    cbor_encoder_create_map(&arr, &sub, 1);
    cbor_encode_text_stringz(&sub, "k");
    cbor_encode_uint(&sub, 1);
    cbor_encoder_close_container(&arr, &sub);
    TEST_ASSERT(cbor_encoder_close_container_checked(&map, &arr) == 0);

    cbor_encode_text_stringz(&map, "itext");
    cbor_encoder_create_indef_text_string(&map, &str);
    cbor_encode_text_stringz(&str, "chunk one, ");
    cbor_encode_text_stringz(&str, "chunk two that is a bit longer");
    cbor_encoder_close_container(&map, &str);

    cbor_encode_text_stringz(&map, "ibytes");
    cbor_encoder_create_indef_byte_string(&map, &str);
    cbor_encode_byte_string(&str, bytes, 30);
    cbor_encode_byte_string(&str, bytes + 30, 34);
    cbor_encoder_close_container(&map, &str);

    cbor_encode_text_stringz(&map, "last");
    cbor_encode_boolean(&map, true);

    TEST_ASSERT_FATAL(cbor_encoder_close_container(enc, &map) == 0);
}

static void
test_encode_flat(void)
{
    CborEncoder enc;

    cbor_encoder_init(&enc, test_flat, sizeof(test_flat), 0);
    test_encode_doc(&enc);
    test_flat_len = cbor_encoder_get_buffer_size(&enc, test_flat);
    TEST_ASSERT_FATAL(cbor_encoder_get_extra_bytes_needed(&enc) == 0);
}

/* Copies flat test document to mbuf chain after prefix of prefix_len bytes */
static struct os_mbuf *
test_mbuf_from_flat(int prefix_len)
{
    static const uint8_t prefix[16];
    struct os_mbuf *om;

    TEST_ASSERT_FATAL(prefix_len <= sizeof(prefix));

    om = test_mbuf_get();
    TEST_ASSERT_FATAL(os_mbuf_append(om, prefix, prefix_len) == 0);
    TEST_ASSERT_FATAL(os_mbuf_append(om, test_flat, test_flat_len) == 0);
    TEST_ASSERT_FATAL(SLIST_NEXT(om, om_next) != NULL);

    return om;
}

static void
test_copy_string(const CborValue *value, uint8_t *buf, size_t *len)
{
    CborError err;

    if (cbor_value_is_text_string(value)) {
        err = cbor_mbuf_value_copy_text_string(value, (char *)buf, len, NULL);
    } else {
        err = cbor_mbuf_value_copy_byte_string(value, buf, len, NULL);
    }
    TEST_ASSERT_FATAL(err == CborNoError, "err=%d", err);
}

/* Finds value for key in map using mbuf helpers */
static void
test_find_value(const CborValue *map, const char *key, CborValue *val)
{
    char buf[16];
    CborValue it;
    size_t len;

    TEST_ASSERT_FATAL(cbor_value_enter_container(map, &it) == 0);
    while (!cbor_value_at_end(&it)) {
        len = sizeof(buf);
        TEST_ASSERT_FATAL(cbor_mbuf_value_copy_text_string(&it, buf, &len, &it) == 0);
        if (strcmp(buf, key) == 0) {
            *val = it;
            return;
        }
        TEST_ASSERT_FATAL(cbor_mbuf_value_advance(&it) == 0);
    }

    TEST_ASSERT_FATAL(0, "key %s not found", key);
}

/* Walks two iterators in parallel and checks they provide same values */
static void
test_compare_values(CborValue *a, CborValue *b)
{
    uint8_t buf_a[128];
    uint8_t buf_b[128];
    CborValue ca, cb;
    uint64_t ua, ub;
    size_t len_a, len_b;
    CborError err;

    while (!cbor_value_at_end(a)) {
        TEST_ASSERT_FATAL(!cbor_value_at_end(b));
        TEST_ASSERT_FATAL(cbor_value_get_type(a) == cbor_value_get_type(b));

        switch (cbor_value_get_type(a)) {
        case CborIntegerType:
            cbor_value_get_raw_integer(a, &ua);
            cbor_value_get_raw_integer(b, &ub);
            TEST_ASSERT(ua == ub);
            TEST_ASSERT(cbor_value_is_negative_integer(a) ==
                        cbor_value_is_negative_integer(b));
            break;
        case CborTextStringType:
        case CborByteStringType:
            len_a = sizeof(buf_a);
            len_b = sizeof(buf_b);
            test_copy_string(a, buf_a, &len_a);
            test_copy_string(b, buf_b, &len_b);
            TEST_ASSERT(len_a == len_b);
            TEST_ASSERT(memcmp(buf_a, buf_b, len_a) == 0);

            TEST_ASSERT(cbor_mbuf_value_calculate_string_length(a, &len_b) ==
                        CborNoError);
            TEST_ASSERT(len_a == len_b);
            break;
        case CborArrayType:
        case CborMapType:
            TEST_ASSERT_FATAL(cbor_value_enter_container(a, &ca) == 0);
            TEST_ASSERT_FATAL(cbor_value_enter_container(b, &cb) == 0);
            test_compare_values(&ca, &cb);
            TEST_ASSERT_FATAL(cbor_value_leave_container(a, &ca) == 0);
            TEST_ASSERT_FATAL(cbor_value_leave_container(b, &cb) == 0);
            continue;
        default:
            break;
        }

        err = cbor_mbuf_value_advance(a);
        TEST_ASSERT_FATAL(err == CborNoError, "err=%d", err);
        err = cbor_mbuf_value_advance(b);
        TEST_ASSERT_FATAL(err == CborNoError, "err=%d", err);
    }

    TEST_ASSERT(cbor_value_at_end(b));
}

TEST_CASE_SELF(test_tinycbor_mbuf_writer)
{
    struct cbor_mbuf_writer writer;
    CborEncoder enc;
    struct os_mbuf *om;
    uint8_t buf[512];

    test_encode_flat();

    om = test_mbuf_get();
    cbor_mbuf_writer_init(&writer, om);
    cbor_writer_encoder_init(&writer.enc, &enc);
    test_encode_doc(&enc);

    TEST_ASSERT(cbor_encode_bytes_written(&enc) == test_flat_len);
    TEST_ASSERT_FATAL(OS_MBUF_PKTLEN(om) == test_flat_len);
    TEST_ASSERT(SLIST_NEXT(om, om_next) != NULL);
    TEST_ASSERT(os_mbuf_copydata(om, 0, test_flat_len, buf) == 0);
    TEST_ASSERT(memcmp(buf, test_flat, test_flat_len) == 0);

    os_mbuf_free_chain(om);
    test_mbuf_check_free();
}

TEST_CASE_SELF(test_tinycbor_mbuf_writer_nomem)
{
    struct cbor_mbuf_writer writer;
    struct os_mbuf *om[TEST_MBUF_COUNT];
    CborEncoder enc;
    int i;

    /* Leave only one mbuf in pool */
    for (i = 0; i < TEST_MBUF_COUNT - 1; i++) {
        om[i] = test_mbuf_get();
    }

    om[i] = test_mbuf_get();
    cbor_mbuf_writer_init(&writer, om[i]);
    cbor_writer_encoder_init(&writer.enc, &enc);
    TEST_ASSERT(cbor_encode_byte_string(&enc, (const uint8_t *)test_text,
                                        sizeof(test_text)) == CborErrorOutOfMemory);

    for (i = 0; i < TEST_MBUF_COUNT; i++) {
        os_mbuf_free_chain(om[i]);
    }
    test_mbuf_check_free();
}

TEST_CASE_SELF(test_tinycbor_cnt_writer)
{
    struct cbor_cnt_writer writer;
    CborEncoder enc;

    test_encode_flat();

    cbor_cnt_writer_init(&writer);
    cbor_writer_encoder_init(&writer.enc, &enc);
    test_encode_doc(&enc);

    TEST_ASSERT(cbor_encode_bytes_written(&enc) == test_flat_len);
}

TEST_CASE_SELF(test_tinycbor_byte_iovec)
{
    static const size_t lens[] = { 0, 5, 23, 24, 255, 256, 300 };
    struct cbor_iovec iov[3];
    uint8_t data[300];
    uint8_t exp[320];
    uint8_t buf[320];
    CborEncoder enc, arr;
    size_t exp_len;
    size_t len;
    int i;

    for (i = 0; i < sizeof(data); i++) {
        data[i] = i;
    }

    for (i = 0; i < ARRAY_SIZE(lens); i++) {
        len = lens[i];

        cbor_encoder_init(&enc, exp, sizeof(exp), 0);
        cbor_encoder_create_array(&enc, &arr, 2);
        cbor_encode_byte_string(&arr, data, len);
        cbor_encode_uint(&arr, 1);
        TEST_ASSERT_FATAL(cbor_encoder_close_container_checked(&enc, &arr) == 0);
        exp_len = cbor_encoder_get_buffer_size(&enc, exp);

        /* split data in 3 chunks, some may be empty */
        iov[0].iov_base = data;
        iov[0].iov_len = len / 3;
        iov[1].iov_base = data + iov[0].iov_len;
        iov[1].iov_len = len / 2;
        iov[2].iov_base = data + iov[0].iov_len + iov[1].iov_len;
        iov[2].iov_len = len - iov[0].iov_len - iov[1].iov_len;

        cbor_encoder_init(&enc, buf, sizeof(buf), 0);
        cbor_encoder_create_array(&enc, &arr, 2);
        TEST_ASSERT(cbor_encode_byte_iovec(&arr, iov, 3) == CborNoError);
        cbor_encode_uint(&arr, 1);
        /* checked close verifies item was counted */
        TEST_ASSERT(cbor_encoder_close_container_checked(&enc, &arr) == 0,
                    "len=%d", (int)len);

        TEST_ASSERT(cbor_encoder_get_buffer_size(&enc, buf) == exp_len);
        TEST_ASSERT(memcmp(buf, exp, exp_len) == 0, "len=%d", (int)len);
    }
}

TEST_CASE_SELF(test_tinycbor_indef_strings)
{
    CborParser parser;
    CborValue it, val;
    char text[64];
    uint8_t bytes[64];
    size_t len;
    int i;

    test_encode_flat();

    TEST_ASSERT_FATAL(cbor_parser_init(test_flat, test_flat_len, 0, &parser, &it) == 0);
    TEST_ASSERT(cbor_value_validate(&it, CborValidateCompleteData) == 0);

    TEST_ASSERT_FATAL(cbor_value_map_find_value(&it, "itext", &val) == 0);
    TEST_ASSERT(cbor_value_is_text_string(&val));
    TEST_ASSERT(!cbor_value_is_length_known(&val));
    len = sizeof(text);
    TEST_ASSERT(cbor_value_copy_text_string(&val, text, &len, NULL) == 0);
    TEST_ASSERT(strcmp(text, "chunk one, chunk two that is a bit longer") == 0);

    TEST_ASSERT_FATAL(cbor_value_map_find_value(&it, "ibytes", &val) == 0);
    TEST_ASSERT(cbor_value_is_byte_string(&val));
    TEST_ASSERT(!cbor_value_is_length_known(&val));
    len = sizeof(bytes);
    TEST_ASSERT(cbor_value_copy_byte_string(&val, bytes, &len, NULL) == 0);
    TEST_ASSERT(len == 64);
    for (i = 0; i < len; i++) {
        TEST_ASSERT(bytes[i] == i);
    }
}

TEST_CASE_SELF(test_tinycbor_mbuf_reader)
{
    struct cbor_mbuf_reader reader;
    uint8_t scratch[sizeof(test_text)];
    CborParser flat_parser, mbuf_parser;
    CborValue flat_it, mbuf_it;
    struct os_mbuf *om;
    int prefix;

    test_encode_flat();

    /* different alignment of data within mbufs */
    for (prefix = 0; prefix < 16; prefix += 5) {
        om = test_mbuf_from_flat(prefix);

        TEST_ASSERT_FATAL(cbor_parser_init(test_flat, test_flat_len, 0,
                                           &flat_parser, &flat_it) == 0);
        cbor_mbuf_reader_init(&reader, om, prefix);
        TEST_ASSERT_FATAL(
            cbor_reader_parser_init(&reader.r, &mbuf_parser, &mbuf_it) == 0);

        test_compare_values(&flat_it, &mbuf_it);

        /* validation also works with reader if strings fit in scratch */
        reader.r.scratch = scratch;
        reader.r.scratch_size = sizeof(scratch);
        cbor_reader_parser_init(&reader.r, &mbuf_parser, &mbuf_it);
        TEST_ASSERT(cbor_value_validate(&mbuf_it, CborValidateCompleteData) == 0);

        os_mbuf_free_chain(om);
    }

    test_mbuf_check_free();
}

TEST_CASE_SELF(test_tinycbor_mbuf_reader_iterators)
{
    struct cbor_mbuf_reader reader;
    CborParser parser;
    CborValue it, map, copy;
    struct os_mbuf *om;
    char key[16];
    size_t len;
    int i;

    test_encode_flat();
    om = test_mbuf_from_flat(3);

    cbor_mbuf_reader_init(&reader, om, 3);
    TEST_ASSERT_FATAL(cbor_reader_parser_init(&reader.r, &parser, &it) == 0);
    TEST_ASSERT_FATAL(cbor_value_enter_container(&it, &map) == 0);

    /* advancing copy of iterator does not affect original */
    copy = map;
    for (i = 0; i < 9; i++) {
        TEST_ASSERT_FATAL(cbor_value_advance(&copy) == 0);
    }

    len = sizeof(key);
    TEST_ASSERT(cbor_mbuf_value_copy_text_string(&map, key, &len, NULL) == 0);
    TEST_ASSERT(strcmp(key, "u8") == 0);

    /* copying string without next iterator does not advance value */
    len = sizeof(key);
    TEST_ASSERT(cbor_mbuf_value_copy_text_string(&map, key, &len, NULL) == 0);
    TEST_ASSERT(strcmp(key, "u8") == 0);

    os_mbuf_free_chain(om);
    test_mbuf_check_free();
}

TEST_CASE_SELF(test_tinycbor_mbuf_reader_strings)
{
    struct cbor_mbuf_reader reader;
    CborParser parser;
    CborValue it, val, next;
    struct os_mbuf *om;
    uint8_t scratch[sizeof(test_text)];
    char text[sizeof(test_text)];
    size_t len;

    test_encode_flat();
    om = test_mbuf_from_flat(0);

    cbor_mbuf_reader_init(&reader, om, 0);
    TEST_ASSERT_FATAL(cbor_reader_parser_init(&reader.r, &parser, &it) == 0);

    test_find_value(&it, "text", &val);
    TEST_ASSERT_FATAL(cbor_value_is_text_string(&val));

    /* string spanning multiple mbufs is not available by pointer */
    len = sizeof(text);
    TEST_ASSERT(cbor_value_copy_text_string(&val, text, &len, NULL) == CborErrorIO);
    next = val;
    TEST_ASSERT(cbor_value_advance(&next) == CborErrorIO);

    /* but can be skipped with helper */
    next = val;
    TEST_ASSERT(cbor_mbuf_value_advance(&next) == CborNoError);
    TEST_ASSERT(cbor_value_is_text_string(&next));

    /* buffer too small */
    len = sizeof(text) - 2;
    TEST_ASSERT(cbor_mbuf_value_copy_text_string(&val, text, &len, NULL) ==
                CborErrorOutOfMemory);
    TEST_ASSERT(len == strlen(test_text));

    /* exact fit, no space for NUL */
    len = strlen(test_text);
    memset(text, 0xff, sizeof(text));
    TEST_ASSERT(cbor_mbuf_value_copy_text_string(&val, text, &len, NULL) == CborNoError);
    TEST_ASSERT(len == strlen(test_text));
    TEST_ASSERT(memcmp(text, test_text, len) == 0);
    TEST_ASSERT(text[len] == (char)0xff);

    /* with space for NUL, next iterator set */
    len = sizeof(text);
    TEST_ASSERT(cbor_mbuf_value_copy_text_string(&val, text, &len, &next) == CborNoError);
    TEST_ASSERT(strcmp(text, test_text) == 0);
    TEST_ASSERT(cbor_value_is_text_string(&next));

    /* byte string helper rejects text string */
    len = sizeof(text);
    TEST_ASSERT(cbor_mbuf_value_copy_byte_string(&val, (uint8_t *)text, &len,
                                                 NULL) == CborErrorIllegalType);

    /* with scratch buffer TinyCBOR functions work for any string */
    reader.r.scratch = scratch;
    reader.r.scratch_size = sizeof(scratch);
    len = sizeof(text);
    TEST_ASSERT(cbor_value_copy_text_string(&val, text, &len, NULL) == CborNoError);
    TEST_ASSERT(strcmp(text, test_text) == 0);

    os_mbuf_free_chain(om);
    test_mbuf_check_free();
}

TEST_CASE_SELF(test_tinycbor_mbuf_reader_truncated)
{
    struct cbor_mbuf_reader reader;
    uint8_t scratch[sizeof(test_text)];
    CborParser parser;
    CborValue it;
    struct os_mbuf *om;
    int len;

    test_encode_flat();

    for (len = 1; len < test_flat_len; len += 7) {
        om = test_mbuf_get();
        TEST_ASSERT_FATAL(os_mbuf_append(om, test_flat, len) == 0);

        cbor_mbuf_reader_init(&reader, om, 0);
        reader.r.scratch = scratch;
        reader.r.scratch_size = sizeof(scratch);
        if (cbor_reader_parser_init(&reader.r, &parser, &it) == 0) {
            TEST_ASSERT(cbor_value_validate(&it, CborValidateCompleteData) ==
                            CborErrorUnexpectedEOF,
                        "len=%d", len);
            cbor_reader_parser_init(&reader.r, &parser, &it);
            TEST_ASSERT(cbor_mbuf_value_advance(&it) == CborErrorUnexpectedEOF,
                        "len=%d", len);
        }

        os_mbuf_free_chain(om);
    }

    test_mbuf_check_free();
}

TEST_CASE_SELF(test_tinycbor_mbuf_helpers_flat)
{
    CborParser parser;
    CborValue it, val;
    char text[sizeof(test_text)];
    size_t len;

    test_encode_flat();

    TEST_ASSERT_FATAL(cbor_parser_init(test_flat, test_flat_len, 0, &parser, &it) == 0);
    TEST_ASSERT_FATAL(cbor_value_map_find_value(&it, "text", &val) == 0);

    TEST_ASSERT(cbor_mbuf_value_calculate_string_length(&val, &len) == 0);
    TEST_ASSERT(len == strlen(test_text));
    len = sizeof(text);
    TEST_ASSERT(cbor_mbuf_value_copy_text_string(&val, text, &len, NULL) == 0);
    TEST_ASSERT(strcmp(text, test_text) == 0);
}

TEST_SUITE(test_tinycbor_suite)
{
    test_mbuf_init();

    test_tinycbor_mbuf_writer();
    test_tinycbor_mbuf_writer_nomem();
    test_tinycbor_cnt_writer();
    test_tinycbor_byte_iovec();
    test_tinycbor_indef_strings();
    test_tinycbor_mbuf_reader();
    test_tinycbor_mbuf_reader_iterators();
    test_tinycbor_mbuf_reader_strings();
    test_tinycbor_mbuf_reader_truncated();
    test_tinycbor_mbuf_helpers_flat();
}

#if MYNEWT_VAL(SELFTEST)
int
main(int argc, char **argv)
{
    sysinit();

    test_tinycbor_suite();

    return tu_any_failed;
}
#endif
