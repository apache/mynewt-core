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
#include <oic/oc_api.h>
#include <oic/port/mynewt/ip.h>
#include "test_oic.h"

/* Internal API of CoAP engine */
int coap_receive(struct os_mbuf **mp);

static struct os_mbuf *
test_coap_mbuf(const uint8_t *data, size_t len)
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

static void
test_coap_rx(const uint8_t *data, size_t len)
{
    struct os_mbuf *m;

    m = test_coap_mbuf(data, len);
    coap_receive(&m);
    if (m) {
        os_mbuf_free_chain(m);
    }
}

static int
test_coap_tran_num_free(void)
{
    struct os_mempool_info omi;

    TEST_ASSERT_FATAL(os_mempool_get("coap_tran", &omi) != NULL);
    TEST_ASSERT(omi.omi_num_free <= omi.omi_num_blocks);

    return omi.omi_num_free;
}

void
test_coap_transaction(void)
{
    /* NON GET request for unknown resource "x" */
    static const uint8_t non_get[] = {
        0x51, 0x01, 0x12, 0x34, 0xaa, 0xb1, 'x',
    };
    /* Message with invalid token length of 9 */
    static const uint8_t bad_tkl[] = {
        0x59, 0x01, 0x12, 0x35, 0x01, 0x02, 0x03,
        0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    };
    int num_free;
    int i;

    num_free = test_coap_tran_num_free();

    /*
     * Transaction used for response is freed when response is sent and
     * shall not be freed again by following requests.
     */
    for (i = 0; i < 2; i++) {
        test_coap_rx(non_get, sizeof(non_get));
        TEST_ASSERT(test_coap_tran_num_free() == num_free);

        test_coap_rx(bad_tkl, sizeof(bad_tkl));
        TEST_ASSERT(test_coap_tran_num_free() == num_free);
    }
}
