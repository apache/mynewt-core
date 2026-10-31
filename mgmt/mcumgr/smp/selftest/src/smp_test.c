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
#include "testutil/testutil.h"
#include "mgmt/mgmt.h"
#include "smp/smp.h"
#include "cborattr/cborattr.h"
#include "os_mgmt/os_mgmt.h"

#define SMP_TEST_GROUP    MGMT_GROUP_ID_PERUSER
#define SMP_TEST_MAX_RSPS 4

static int smp_test_handler_calls;
static struct os_mbuf *smp_test_rsps[SMP_TEST_MAX_RSPS];
static int smp_test_num_rsps;

static int
smp_test_handler(struct mgmt_ctxt *ctxt)
{
    smp_test_handler_calls++;
    return 0;
}

static const struct mgmt_handler smp_test_handlers[] = {
    [0] = { smp_test_handler, smp_test_handler },
};

static struct mgmt_group smp_test_group = {
    .mg_handlers = smp_test_handlers,
    .mg_handlers_count = 1,
    .mg_group_id = SMP_TEST_GROUP,
};

static int
smp_test_tx_rsp(struct smp_streamer *ss, struct os_mbuf *rsp, void *arg)
{
    TEST_ASSERT_FATAL(smp_test_num_rsps < SMP_TEST_MAX_RSPS);
    smp_test_rsps[smp_test_num_rsps++] = rsp;
    return 0;
}

static void
smp_test_reset(void)
{
    int i;

    for (i = 0; i < smp_test_num_rsps; i++) {
        os_mbuf_free_chain(smp_test_rsps[i]);
        smp_test_rsps[i] = NULL;
    }
    smp_test_num_rsps = 0;
    smp_test_handler_calls = 0;
}

/**
 * Appends a request to the supplied mbuf.  The header length field is set to
 * hdr_len and the payload is an empty CBOR map followed by pad_len filler
 * bytes.
 */
static void
smp_test_append_req_id(struct os_mbuf *om, uint16_t group_id, uint8_t id,
                       uint8_t seq, uint16_t hdr_len, uint16_t pad_len)
{
    const uint8_t empty_map = 0xa0;
    struct mgmt_hdr hdr = {
        .nh_op = MGMT_OP_READ,
        .nh_len = hdr_len,
        .nh_group = group_id,
        .nh_seq = seq,
        .nh_id = id,
    };
    int rc;

    mgmt_hton_hdr(&hdr);
    rc = os_mbuf_append(om, &hdr, sizeof(hdr));
    TEST_ASSERT_FATAL(rc == 0);
    rc = os_mbuf_append(om, &empty_map, sizeof(empty_map));
    TEST_ASSERT_FATAL(rc == 0);
    while (pad_len-- > 0) {
        rc = os_mbuf_append(om, &empty_map, sizeof(empty_map));
        TEST_ASSERT_FATAL(rc == 0);
    }
}

static void
smp_test_append_req(struct os_mbuf *om, uint16_t group_id, uint8_t seq,
                    uint16_t hdr_len, uint16_t pad_len)
{
    smp_test_append_req_id(om, group_id, 0, seq, hdr_len, pad_len);
}

static void
smp_test_process(struct os_mbuf *om)
{
    struct cbor_mbuf_reader reader;
    struct cbor_mbuf_writer writer;
    struct smp_streamer streamer = {
        .reader = &reader,
        .writer = &writer,
        .tx_rsp_cb = smp_test_tx_rsp,
    };

    smp_process_request_packet(&streamer, om);
}

static uint8_t
smp_test_rsp_seq(int idx)
{
    struct mgmt_hdr hdr;
    int rc;

    rc = os_mbuf_copydata(smp_test_rsps[idx], 0, sizeof(hdr), &hdr);
    TEST_ASSERT_FATAL(rc == 0);

    return hdr.nh_seq;
}

static struct os_mbuf *
smp_test_get_om(void)
{
    struct os_mbuf *om;

    om = os_msys_get_pkthdr(0, 0);
    TEST_ASSERT_FATAL(om != NULL);

    return om;
}

TEST_CASE(smp_test_case_complete_req)
{
    struct os_mbuf *om;

    smp_test_reset();

    om = smp_test_get_om();
    smp_test_append_req(om, SMP_TEST_GROUP, 1, 1, 0);
    smp_test_process(om);

    TEST_ASSERT(smp_test_handler_calls == 1);
    TEST_ASSERT_FATAL(smp_test_num_rsps == 1);
    TEST_ASSERT(smp_test_rsp_seq(0) == 1);

    smp_test_reset();
}

TEST_CASE(smp_test_case_truncated_req)
{
    struct os_mbuf *om;

    smp_test_reset();

    /* Header claims more payload than the packet contains. */
    om = smp_test_get_om();
    smp_test_append_req(om, SMP_TEST_GROUP, 2, 20, 0);
    smp_test_process(om);

    TEST_ASSERT(smp_test_handler_calls == 0);
    TEST_ASSERT(smp_test_num_rsps == 0);

    /* Header itself is incomplete. */
    om = smp_test_get_om();
    smp_test_append_req(om, SMP_TEST_GROUP, 3, 1, 0);
    os_mbuf_adj(om, -(int)(OS_MBUF_PKTLEN(om) - 5));
    smp_test_process(om);

    TEST_ASSERT(smp_test_handler_calls == 0);
    TEST_ASSERT(smp_test_num_rsps == 0);

    smp_test_reset();
}

TEST_CASE(smp_test_case_truncated_second_req)
{
    struct os_mbuf *om;

    smp_test_reset();

    /* First request is complete and padded to a multiple of 4, second one is
     * truncated.  Only the first one may be processed.
     */
    om = smp_test_get_om();
    smp_test_append_req(om, SMP_TEST_GROUP, 4, 1, 3);
    smp_test_append_req(om, SMP_TEST_GROUP, 5, 10, 0);
    smp_test_process(om);

    TEST_ASSERT(smp_test_handler_calls == 1);
    TEST_ASSERT_FATAL(smp_test_num_rsps == 1);
    TEST_ASSERT(smp_test_rsp_seq(0) == 4);

    smp_test_reset();
}

TEST_CASE(smp_test_case_mcumgr_params)
{
    struct os_mbuf *om;
    long long unsigned int buf_size = 0;
    long long unsigned int buf_count = 0;
    long long int rc_val = -1;
    const struct cbor_attr_t attrs[] = {
        {
         .attribute = "buf_size",
         .type = CborAttrUnsignedIntegerType,
         .addr.uinteger = &buf_size,
         .nodefault = 1,
         },
        {
         .attribute = "buf_count",
         .type = CborAttrUnsignedIntegerType,
         .addr.uinteger = &buf_count,
         .nodefault = 1,
         },
        {
         .attribute = "rc",
         .type = CborAttrIntegerType,
         .addr.integer = &rc_val,
         .nodefault = 1,
         },
        { 0 }
    };
    int rc;

    smp_test_reset();

    om = smp_test_get_om();
    smp_test_append_req_id(om, MGMT_GROUP_ID_OS, OS_MGMT_ID_MCUMGR_PARAMS, 6, 1, 0);
    smp_test_process(om);

    TEST_ASSERT_FATAL(smp_test_num_rsps == 1);
    TEST_ASSERT(smp_test_rsp_seq(0) == 6);

    rc = cbor_read_mbuf_attrs(smp_test_rsps[0], MGMT_HDR_SIZE,
                              OS_MBUF_PKTLEN(smp_test_rsps[0]) - MGMT_HDR_SIZE,
                              attrs);
    TEST_ASSERT_FATAL(rc == 0);
    TEST_ASSERT(rc_val == -1);
    TEST_ASSERT(buf_size == MGMT_MAX_MTU);
    TEST_ASSERT(buf_count == MYNEWT_VAL(OS_MGMT_MCUMGR_PARAMS_BUF_COUNT));

    smp_test_reset();
}

TEST_SUITE(smp_test_suite)
{
    smp_test_case_complete_req();
    smp_test_case_truncated_req();
    smp_test_case_truncated_second_req();
    smp_test_case_mcumgr_params();
}

int
main(int argc, char **argv)
{
    sysinit();

    mgmt_register_group(&smp_test_group);

    smp_test_suite();

    return tu_any_failed;
}
