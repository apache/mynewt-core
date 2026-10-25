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
#include <stdio.h>
#include "os/mynewt.h"
#include <tinycbor/cbor.h>
#include <oic/oc_api.h>
#include <oic/oc_rep.h>
#include <oic/messaging/coap/coap.h>
#include <oic/port/mynewt/ip.h>
#include "test_oic.h"

static struct os_mbuf *
test_parse_mbuf(const uint8_t *data, size_t len)
{
    struct os_mbuf *m;
    int rc;

    m = os_msys_get_pkthdr(len, sizeof(struct oc_endpoint_ip));
    TEST_ASSERT_FATAL(m != NULL);
    memset(OC_MBUF_ENDPOINT(m), 0, sizeof(struct oc_endpoint_ip));
    OC_MBUF_ENDPOINT(m)->ep.oe_type = oc_ip6_transport_id;

    rc = os_mbuf_append(m, data, len);
    TEST_ASSERT_FATAL(rc == 0);

    return m;
}

static coap_status_t
test_parse_coap(const uint8_t *data, size_t len, struct coap_packet_rx *pkt)
{
    struct os_mbuf *m;
    coap_status_t rc;

    m = test_parse_mbuf(data, len);
    rc = coap_parse_message(pkt, &m);
    os_mbuf_free_chain(m);

    return rc;
}

static void
test_parse_coap_options(void)
{
    /* Extended option length larger than packet */
    static const uint8_t ext_len[] = { 0x40, 0x01, 0x00, 0x01, 0x0e, 0xfe, 0xf0 };
    /* Reserved option length */
    static const uint8_t rsv_len[] = { 0x40, 0x01, 0x00, 0x01, 0x0f };
    /* Uri-Path option longer than packet */
    static const uint8_t long_path[] = { 0x40, 0x01, 0x00, 0x01, 0xb4, 'a' };
    /* Two Uri-Path options, second one longer than packet */
    static const uint8_t long_path2[] = { 0x40, 0x01, 0x00, 0x01, 0xb1,
                                          'a',  0x0d, 0xff, 'b' };
    /* Uri-Path "a/bc" */
    static const uint8_t path[] = { 0x40, 0x01, 0x00, 0x01, 0xb1,
                                    'a',  0x02, 'b',  'c' };
    struct coap_packet_rx pkt;
    struct os_mbuf *m;
    char buf[8];
    int rc;

    rc = test_parse_coap(ext_len, sizeof(ext_len), &pkt);
    TEST_ASSERT(rc == BAD_REQUEST_4_00);

    rc = test_parse_coap(rsv_len, sizeof(rsv_len), &pkt);
    TEST_ASSERT(rc == BAD_REQUEST_4_00);

    rc = test_parse_coap(long_path, sizeof(long_path), &pkt);
    TEST_ASSERT(rc == BAD_REQUEST_4_00);

    rc = test_parse_coap(long_path2, sizeof(long_path2), &pkt);
    TEST_ASSERT(rc == BAD_REQUEST_4_00);

    m = test_parse_mbuf(path, sizeof(path));
    rc = coap_parse_message(&pkt, &m);
    TEST_ASSERT(rc == NO_ERROR);
    TEST_ASSERT(pkt.uri_path_len == 4);
    rc = os_mbuf_copydata(m, pkt.uri_path_off, pkt.uri_path_len, buf);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(!memcmp(buf, "a/bc", 4));
    os_mbuf_free_chain(m);
}

static int
test_parse_rep(const uint8_t *data, size_t len, oc_rep_t **rep)
{
    struct os_mbuf *m;
    int rc;

    m = test_parse_mbuf(data, len);
    rc = oc_parse_rep(m, 0, len, rep);
    os_mbuf_free_chain(m);

    return rc;
}

static uint8_t test_parse_buf[1024];

/* Encodes {"a": [cnt elements of type]} */
static size_t
test_parse_encode_array(CborType type, int cnt)
{
    CborEncoder enc;
    CborEncoder map;
    CborEncoder arr;
    int rc;
    int i;

    cbor_encoder_init(&enc, test_parse_buf, sizeof(test_parse_buf), 0);
    rc = cbor_encoder_create_map(&enc, &map, 1);
    rc |= cbor_encode_text_stringz(&map, "a");
    rc |= cbor_encoder_create_array(&map, &arr, cnt);
    for (i = 0; i < cnt; i++) {
        switch (type) {
        case CborIntegerType:
            rc |= cbor_encode_int(&arr, i);
            break;
        case CborTextStringType:
            rc |= cbor_encode_text_stringz(&arr, "abcdefghijklmnopqrstuvw");
            break;
        default:
            TEST_ASSERT_FATAL(0);
        }
    }
    rc |= cbor_encoder_close_container(&map, &arr);
    rc |= cbor_encoder_close_container(&enc, &map);
    TEST_ASSERT_FATAL(rc == 0);

    return cbor_encoder_get_buffer_size(&enc, test_parse_buf);
}

static void
test_parse_rep_arrays(void)
{
    oc_rep_t *rep;
    size_t len;
    int rc;
    int i;

    /* Element count not fitting in 8 bits */
    len = test_parse_encode_array(CborIntegerType, 257);
    rc = test_parse_rep(test_parse_buf, len, &rep);
    TEST_ASSERT_FATAL(rc == 0);
    TEST_ASSERT_FATAL(rep != NULL && rep->type == INT_ARRAY);
    TEST_ASSERT_FATAL(oc_int_array_size(rep->value_array) == 257);
    for (i = 0; i < 257; i++) {
        TEST_ASSERT(oc_int_array(rep->value_array)[i] == i);
    }
    oc_free_rep(rep);

    /* Strings of maximum length */
    len = test_parse_encode_array(CborTextStringType, 3);
    rc = test_parse_rep(test_parse_buf, len, &rep);
    TEST_ASSERT_FATAL(rc == 0);
    TEST_ASSERT_FATAL(rep != NULL && rep->type == STRING_ARRAY);
    for (i = 0; i < 3; i++) {
        TEST_ASSERT(!strcmp(oc_string_array_get_item(rep->value_array, i),
                            "abcdefghijklmnopqrstuvw"));
    }
    oc_free_rep(rep);
}

/* Encodes {"a0": 0, "a1": 1, ...} */
static size_t
test_parse_encode_map(int cnt)
{
    CborEncoder enc;
    CborEncoder map;
    char key[16];
    int rc;
    int i;

    cbor_encoder_init(&enc, test_parse_buf, sizeof(test_parse_buf), 0);
    rc = cbor_encoder_create_map(&enc, &map, cnt);
    for (i = 0; i < cnt; i++) {
        snprintf(key, sizeof(key), "a%d", i);
        rc |= cbor_encode_text_stringz(&map, key);
        rc |= cbor_encode_int(&map, i);
    }
    rc |= cbor_encoder_close_container(&enc, &map);
    TEST_ASSERT_FATAL(rc == 0);

    return cbor_encoder_get_buffer_size(&enc, test_parse_buf);
}

static void
test_parse_rep_invalid(void)
{
    /* 1 */
    static const uint8_t not_map[] = { 0x01 };
    /* [1] */
    static const uint8_t not_map_arr[] = { 0x81, 0x01 };
    /* {"a": ["x...x"]} with string not fitting in array item */
    static const uint8_t long_str[] = { 0xa1, 0x61, 'a', 0x81, 0x78, 0x18,
                                        'x',  'x',  'x', 'x',  'x',  'x',
                                        'x',  'x',  'x', 'x',  'x',  'x',
                                        'x',  'x',  'x', 'x',  'x',  'x',
                                        'x',  'x',  'x', 'x',  'x',  'x' };
    /* {"a": [false, "A"]} */
    static const uint8_t mixed[] = { 0xa1, 0x61, 'a', 0x82, 0xf4, 0x61, 'A' };
    /* {"a": [1, {}]} */
    static const uint8_t mixed_map[] = { 0xa1, 0x61, 'a', 0x82, 0x01, 0xa0 };
    /* {1: 1} */
    static const uint8_t int_key[] = { 0xa1, 0x01, 0x01 };
    /* {"a": [1, 2 with 5 elements declared */
    static const uint8_t truncated[] = { 0xa1, 0x61, 'a', 0x85, 0x01, 0x02 };
    /* {"a": {"a": {"a": ... 10 levels */
    static const uint8_t nested[] = { 0xa1, 0x61, 'a',  0xa1, 0x61, 'a',  0xa1,
                                      0x61, 'a',  0xa1, 0x61, 'a',  0xa1, 0x61,
                                      'a',  0xa1, 0x61, 'a',  0xa1, 0x61, 'a',
                                      0xa1, 0x61, 'a',  0xa1, 0x61, 'a',  0xa1,
                                      0x61, 'a',  0xa0 };
    oc_rep_t dummy;
    oc_rep_t *rep;
    size_t len;
    int rc;

    rep = &dummy;
    rc = test_parse_rep(not_map, sizeof(not_map), &rep);
    TEST_ASSERT(rc != 0);
    TEST_ASSERT(rep == NULL);

    rc = test_parse_rep(not_map_arr, sizeof(not_map_arr), &rep);
    TEST_ASSERT(rc != 0);
    oc_free_rep(rep);

    rc = test_parse_rep(long_str, sizeof(long_str), &rep);
    TEST_ASSERT(rc != 0);
    oc_free_rep(rep);

    rc = test_parse_rep(mixed, sizeof(mixed), &rep);
    TEST_ASSERT(rc != 0);
    oc_free_rep(rep);

    rc = test_parse_rep(mixed_map, sizeof(mixed_map), &rep);
    TEST_ASSERT(rc != 0);
    oc_free_rep(rep);

    rc = test_parse_rep(int_key, sizeof(int_key), &rep);
    TEST_ASSERT(rc != 0);
    oc_free_rep(rep);

    rc = test_parse_rep(truncated, sizeof(truncated), &rep);
    TEST_ASSERT(rc != 0);
    oc_free_rep(rep);

    rc = test_parse_rep(nested, sizeof(nested), &rep);
    TEST_ASSERT(rc != 0);
    oc_free_rep(rep);

    /* More properties than rep objects */
    len = test_parse_encode_map(MYNEWT_VAL(OC_NUM_REP_OBJECTS) + 1);
    rc = test_parse_rep(test_parse_buf, len, &rep);
    TEST_ASSERT(rc != 0);
    oc_free_rep(rep);
}

static void
test_parse_rep_objects(void)
{
    /* [] */
    static const uint8_t empty[] = { 0x80 };
    /* [{"a": 1}, {"b": ["xy", "z"]}] */
    static const uint8_t objs[] = { 0x82, 0xa1, 0x61, 'a', 0x01, 0xa1, 0x61,
                                    'b',  0x82, 0x62, 'x', 'y',  0x61, 'z' };
    oc_rep_t *rep;
    size_t len;
    int rc;

    rc = test_parse_rep(empty, sizeof(empty), &rep);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(rep == NULL);

    rc = test_parse_rep(objs, sizeof(objs), &rep);
    TEST_ASSERT_FATAL(rc == 0);
    TEST_ASSERT_FATAL(rep != NULL && rep->type == OBJECT);
    TEST_ASSERT_FATAL(rep->value_object != NULL);
    TEST_ASSERT(rep->value_object->type == INT);
    TEST_ASSERT(rep->value_object->value_int == 1);
    TEST_ASSERT_FATAL(rep->next != NULL && rep->next->type == OBJECT);
    TEST_ASSERT_FATAL(rep->next->value_object != NULL);
    TEST_ASSERT_FATAL(rep->next->value_object->type == STRING_ARRAY);
    TEST_ASSERT(oc_string_array_get_allocated_size(
                    rep->next->value_object->value_array) == 2);
    TEST_ASSERT(!strcmp(
        oc_string_array_get_item(rep->next->value_object->value_array, 0), "xy"));
    TEST_ASSERT(!strcmp(
        oc_string_array_get_item(rep->next->value_object->value_array, 1), "z"));
    TEST_ASSERT(rep->next->next == NULL);
    oc_free_rep(rep);

    /* All rep objects were freed after failed and successful parsing */
    len = test_parse_encode_map(MYNEWT_VAL(OC_NUM_REP_OBJECTS));
    rc = test_parse_rep(test_parse_buf, len, &rep);
    TEST_ASSERT(rc == 0);
    oc_free_rep(rep);
}

void
test_parse(void)
{
    test_parse_coap_options();
    test_parse_rep_arrays();
    test_parse_rep_invalid();
    test_parse_rep_objects();
}
