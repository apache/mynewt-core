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
#include <newtmgr/newtmgr.h>

static struct nmgr_transport nmgr_shell_transport;

static uint16_t
nmgr_shell_get_mtu(struct os_mbuf *m)
{
    return MGMT_MAX_MTU;
}

static int
nmgr_shell_out(struct nmgr_transport *nt, struct os_mbuf *m)
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
nmgr_shell_in(struct os_mbuf *m, void *arg)
{
    return nmgr_rx_req(&nmgr_shell_transport, m);
}

void
nmgr_shell_pkg_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = nmgr_transport_init(&nmgr_shell_transport, nmgr_shell_out,
      nmgr_shell_get_mtu);
    assert(rc == 0);

    rc = shell_nlip_input_register(nmgr_shell_in, &nmgr_shell_transport);
    assert(rc == 0);
}
