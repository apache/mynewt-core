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

#include <inttypes.h>
#include <assert.h>

#include "os/mynewt.h"
#include <shell/shell.h>
#include <mgmt/mgmt.h>
#include <mynewt_smp/smp.h>
#include "tinycbor/cbor_mbuf_reader.h"
#include "tinycbor/cbor_mbuf_writer.h"

static struct smp_transport g_smp_shell_transport;

static uint16_t
smp_shell_get_mtu(struct os_mbuf *m)
{
    return MGMT_MAX_MTU;
}

static int
smp_shell_out(struct os_mbuf *m)
{
    int rc;

    rc = shell_nlip_output(m);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    os_mbuf_free_chain(m);
    return (rc);
}

static int
smp_shell_in(struct os_mbuf *m, void *arg)
{
    struct cbor_mbuf_reader cmr;
    struct cbor_mbuf_writer cmw;

    g_smp_shell_transport.st_streamer = (struct smp_streamer) {
        .mgmt_stmr = {
            .cfg = &g_smp_cbor_cfg,
            .reader = &cmr.r,
            .writer = &cmw.enc,
            .cb_arg = &g_smp_shell_transport,
        },
        .tx_rsp_cb = smp_tx_rsp,
    };

    return smp_process_request_packet(&g_smp_shell_transport.st_streamer, m);
}

void
smp_shell_pkg_init(void)
{
    int rc;

    SYSINIT_ASSERT_ACTIVE();

    rc = smp_transport_init(&g_smp_shell_transport, smp_shell_out, smp_shell_get_mtu);
    assert(rc == 0);

    rc = shell_nlip_input_register(smp_shell_in, &g_smp_shell_transport);
    assert(rc == 0);
}
