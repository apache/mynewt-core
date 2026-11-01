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
#include "mynewt_smp/smp.h"

#define SMP_REASM_TEST_USRHDR_LEN 2

static uint8_t smp_reasm_test_buf[MGMT_MAX_MTU + 64];

/**
 * Fills the test buffer with a request whose header announces payload_len
 * bytes; the payload is a byte pattern.
 */
static void
smp_reasm_test_fill(uint16_t payload_len)
{
    struct mgmt_hdr hdr = {
        .nh_op = MGMT_OP_WRITE,
        .nh_len = payload_len,
        .nh_group = MGMT_GROUP_ID_IMAGE,
        .nh_seq = 7,
        .nh_id = 1,
    };
    int i;

    for (i = 0; i < sizeof(smp_reasm_test_buf); i++) {
        smp_reasm_test_buf[i] = i;
    }

    mgmt_hton_hdr(&hdr);
    memcpy(smp_reasm_test_buf, &hdr, sizeof(hdr));
}

static struct os_mbuf *
smp_reasm_test_frag(int off, int len)
{
    struct os_mbuf *om;
    int rc;

    om = os_msys_get_pkthdr(0, 0);
    TEST_ASSERT_FATAL(om != NULL);

    rc = os_mbuf_append(om, smp_reasm_test_buf + off, len);
    TEST_ASSERT_FATAL(rc == 0);

    return om;
}

/**
 * Feeds buffer bytes [off, off + len) as one fragment.
 */
static int
smp_reasm_test_rx(struct smp_reasm *sr, int off, int len, struct os_mbuf **out_req)
{
    struct os_mbuf *frag;
    int rc;

    frag = smp_reasm_test_frag(off, len);
    rc = smp_reasm_rx(sr, frag, SMP_REASM_TEST_USRHDR_LEN, out_req);
    os_mbuf_free_chain(frag);

    return rc;
}

static void
smp_reasm_test_check_req(struct os_mbuf *req, int len)
{
    TEST_ASSERT_FATAL(req != NULL);
    TEST_ASSERT(OS_MBUF_IS_PKTHDR(req));
    TEST_ASSERT(OS_MBUF_USRHDR_LEN(req) == SMP_REASM_TEST_USRHDR_LEN);
    TEST_ASSERT_FATAL(OS_MBUF_PKTLEN(req) == len);
    TEST_ASSERT(os_mbuf_cmpf(req, 0, smp_reasm_test_buf, len) == 0);
}

TEST_CASE(smp_reasm_test_case_single)
{
    struct smp_reasm sr = { 0 };
    struct os_mbuf *req;
    int num_free;
    int rc;

    num_free = os_msys_num_free();

    smp_reasm_test_fill(20);
    TEST_ASSERT(smp_reasm_test_rx(&sr, 0, 28, &req) == 0);
    smp_reasm_test_check_req(req, 28);
    TEST_ASSERT(sr.sr_om == NULL);
    os_mbuf_free_chain(req);

    /* Request without payload. */
    smp_reasm_test_fill(0);
    rc = smp_reasm_test_rx(&sr, 0, MGMT_HDR_SIZE, &req);
    TEST_ASSERT(rc == 0);
    smp_reasm_test_check_req(req, MGMT_HDR_SIZE);
    os_mbuf_free_chain(req);

    TEST_ASSERT(os_msys_num_free() == num_free);
}

TEST_CASE(smp_reasm_test_case_fragmented)
{
    struct smp_reasm sr = { 0 };
    struct os_mbuf *req;
    int num_free;
    int off;
    int len;

    num_free = os_msys_num_free();

    /* Header split over two fragments, payload over several more. */
    smp_reasm_test_fill(500);
    TEST_ASSERT(smp_reasm_test_rx(&sr, 0, 3, &req) == 0);
    TEST_ASSERT(req == NULL);
    TEST_ASSERT(smp_reasm_test_rx(&sr, 3, 17, &req) == 0);
    TEST_ASSERT(req == NULL);

    for (off = 20; off < 508; off += len) {
        len = min(20, 508 - off);
        TEST_ASSERT(smp_reasm_test_rx(&sr, off, len, &req) == 0);
        if (off + len < 508) {
            TEST_ASSERT_FATAL(req == NULL);
        }
    }

    smp_reasm_test_check_req(req, 508);
    TEST_ASSERT(sr.sr_om == NULL);
    os_mbuf_free_chain(req);

    /* Largest allowed request. */
    smp_reasm_test_fill(MGMT_MAX_MTU - MGMT_HDR_SIZE);
    for (off = 0; off < MGMT_MAX_MTU; off += len) {
        len = min(244, MGMT_MAX_MTU - off);
        TEST_ASSERT(smp_reasm_test_rx(&sr, off, len, &req) == 0);
    }
    smp_reasm_test_check_req(req, MGMT_MAX_MTU);
    os_mbuf_free_chain(req);

    TEST_ASSERT(os_msys_num_free() == num_free);
}

TEST_CASE(smp_reasm_test_case_trailing_data)
{
    struct smp_reasm sr = { 0 };
    struct os_mbuf *req;
    int num_free;

    num_free = os_msys_num_free();

    /* Data after the first request is passed on with it. */
    smp_reasm_test_fill(30);
    TEST_ASSERT(smp_reasm_test_rx(&sr, 0, 20, &req) == 0);
    TEST_ASSERT(req == NULL);
    TEST_ASSERT(smp_reasm_test_rx(&sr, 20, 30, &req) == 0);
    smp_reasm_test_check_req(req, 50);
    TEST_ASSERT(sr.sr_om == NULL);
    os_mbuf_free_chain(req);

    TEST_ASSERT(os_msys_num_free() == num_free);
}

TEST_CASE(smp_reasm_test_case_too_long)
{
    struct smp_reasm sr = { 0 };
    struct os_mbuf *req;
    int num_free;
    int rc;

    num_free = os_msys_num_free();

    smp_reasm_test_fill(MGMT_MAX_MTU - MGMT_HDR_SIZE + 1);
    TEST_ASSERT(smp_reasm_test_rx(&sr, 0, 4, &req) == 0);
    TEST_ASSERT(req == NULL);
    rc = smp_reasm_test_rx(&sr, 4, 20, &req);
    TEST_ASSERT(rc == MGMT_ERR_EMSGSIZE);
    TEST_ASSERT(req == NULL);
    TEST_ASSERT(sr.sr_om == NULL);

    TEST_ASSERT(os_msys_num_free() == num_free);
}

TEST_CASE(smp_reasm_test_case_invalid_hdr)
{
    struct smp_reasm sr = { 0 };
    struct mgmt_hdr *hdr;
    struct os_mbuf *req;
    int num_free;
    int rc;

    num_free = os_msys_num_free();
    hdr = (struct mgmt_hdr *)smp_reasm_test_buf;

    /* Response opcode is not a valid request. */
    smp_reasm_test_fill(30);
    hdr->nh_op = MGMT_OP_WRITE_RSP;
    rc = smp_reasm_test_rx(&sr, 0, 20, &req);
    TEST_ASSERT(rc == MGMT_ERR_EINVAL);
    TEST_ASSERT(req == NULL);
    TEST_ASSERT(sr.sr_om == NULL);

    /* Reserved bits must be zero. */
    smp_reasm_test_fill(30);
    hdr->_res1 = 0x04;
    rc = smp_reasm_test_rx(&sr, 0, 20, &req);
    TEST_ASSERT(rc == MGMT_ERR_EINVAL);
    TEST_ASSERT(sr.sr_om == NULL);

    /* SMP version bits are accepted. */
    smp_reasm_test_fill(30);
    hdr->_res1 = 0x01;
    TEST_ASSERT(smp_reasm_test_rx(&sr, 0, 20, &req) == 0);
    TEST_ASSERT(req == NULL);
    TEST_ASSERT(smp_reasm_test_rx(&sr, 20, 18, &req) == 0);
    smp_reasm_test_check_req(req, 38);
    os_mbuf_free_chain(req);

    TEST_ASSERT(os_msys_num_free() == num_free);
}

TEST_CASE(smp_reasm_test_case_resync)
{
    struct smp_reasm sr = { 0 };
    struct os_mbuf *req;
    int num_free;
    int rc;

    num_free = os_msys_num_free();

    /* A fragment from the middle of a request (its first fragment was lost)
     * is dropped and the next request is reassembled correctly.
     */
    smp_reasm_test_fill(200);
    rc = smp_reasm_test_rx(&sr, 100, 20, &req);
    TEST_ASSERT(rc != 0);
    TEST_ASSERT(sr.sr_om == NULL);
    rc = smp_reasm_test_rx(&sr, 120, 20, &req);
    TEST_ASSERT(rc != 0);
    TEST_ASSERT(sr.sr_om == NULL);

    TEST_ASSERT(smp_reasm_test_rx(&sr, 0, 100, &req) == 0);
    TEST_ASSERT(req == NULL);
    TEST_ASSERT(smp_reasm_test_rx(&sr, 100, 108, &req) == 0);
    smp_reasm_test_check_req(req, 208);
    os_mbuf_free_chain(req);

    TEST_ASSERT(os_msys_num_free() == num_free);
}

TEST_CASE(smp_reasm_test_case_reset)
{
    struct smp_reasm sr = { 0 };
    struct os_mbuf *req;
    int num_free;

    num_free = os_msys_num_free();

    /* Partial request is dropped, next request starts from scratch. */
    smp_reasm_test_fill(100);
    TEST_ASSERT(smp_reasm_test_rx(&sr, 0, 50, &req) == 0);
    TEST_ASSERT(req == NULL);
    smp_reasm_reset(&sr);
    TEST_ASSERT(sr.sr_om == NULL);

    smp_reasm_test_fill(10);
    TEST_ASSERT(smp_reasm_test_rx(&sr, 0, 18, &req) == 0);
    smp_reasm_test_check_req(req, 18);
    os_mbuf_free_chain(req);

    /* Resetting idle state is a no-op. */
    smp_reasm_reset(&sr);

    TEST_ASSERT(os_msys_num_free() == num_free);
}

TEST_CASE(smp_reasm_test_case_req_len)
{
    struct os_mbuf *om;

    smp_reasm_test_fill(300);

    om = smp_reasm_test_frag(0, MGMT_HDR_SIZE - 1);
    TEST_ASSERT(smp_reasm_req_len(om) == 0);
    os_mbuf_free_chain(om);

    om = smp_reasm_test_frag(0, MGMT_HDR_SIZE);
    TEST_ASSERT(smp_reasm_req_len(om) == MGMT_HDR_SIZE + 300);
    os_mbuf_free_chain(om);
}

TEST_SUITE(smp_reasm_test_suite)
{
    smp_reasm_test_case_single();
    smp_reasm_test_case_fragmented();
    smp_reasm_test_case_trailing_data();
    smp_reasm_test_case_too_long();
    smp_reasm_test_case_invalid_hdr();
    smp_reasm_test_case_resync();
    smp_reasm_test_case_reset();
    smp_reasm_test_case_req_len();
}

int
main(int argc, char **argv)
{
    sysinit();

    smp_reasm_test_suite();

    return tu_any_failed;
}
