/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include "bsp/bsp.h"
#include "os/os.h"
#include "console/console.h"
#include "shell/shell.h"
#include "newtmgr/newtmgr.h"
#include "host/ble_hs.h"
#include "bleshell_priv.h"

struct nmgr_transport nm_ble_transport;
uint16_t nm_attr_val_handle;

static bssnz_t uint8_t nm_buf[512];

int
nm_chr_access(uint16_t conn_handle, uint16_t attr_handle,
              uint8_t op, union ble_gatt_access_ctxt *ctxt,
              void *arg)
{
    struct nmgr_transport *nt;
    struct os_mbuf *om;
    int rc;

    assert(attr_handle == nm_attr_val_handle);

    om = NULL;

    if (op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        rc = BLE_ATT_ERR_WRITE_NOT_PERMITTED;
        goto err;
    }

    om = os_msys_get_pkthdr(ctxt->chr_access.len, 2);
    if (om == NULL) {
        rc = 1;
        goto err;
    }

    /* Put the connection handle in the request packet header. */
    memcpy(OS_MBUF_USRHDR(om), &conn_handle, sizeof conn_handle);

    rc = os_mbuf_append(om, ctxt->chr_access.data, ctxt->chr_access.len);
    if (rc != 0) {
        goto err;
    }

    nt = arg;
    rc = nmgr_rx_req(nt, om);

    return rc;

err:
    os_mbuf_free_chain(om);
    return rc;
}

static int
nm_ble_out(struct nmgr_transport *nt, struct os_mbuf *om)
{
    struct ble_gatt_attr attr;
    uint16_t conn_handle;
    int rc;

    assert(OS_MBUF_USRHDR_LEN(om) == 2);
    memcpy(&conn_handle, OS_MBUF_USRHDR(om), 2);

    assert(OS_MBUF_PKTLEN(om) <= sizeof nm_buf);
    rc = os_mbuf_copydata(om, 0, OS_MBUF_PKTLEN(om), nm_buf);
    assert(rc == 0);

    attr.handle = nm_attr_val_handle;
    attr.offset = 0;
    attr.value_len = OS_MBUF_PKTLEN(om);
    attr.value = nm_buf;

    rc = ble_gattc_notify_custom(conn_handle, &attr);
    console_printf("nm_ble_out(); conn_handle = %d notify-rc=%d\n",
                   conn_handle, rc);

    return rc;
}

int
nm_rx_rsp(uint8_t *attr_val, uint16_t attr_len)
{
    struct os_mbuf *om;
    int rc;

    om = os_msys_get_pkthdr(attr_len, 0);
    if (om == NULL) {
        rc = 1;
        goto err;
    }

    rc = os_mbuf_append(om, attr_val, attr_len);
    if (rc != 0) {
        goto err;
    }

    bleshell_printf("received nmgr rsp: ");
    rc = shell_nlip_output(om);
    if (rc != 0) {
        goto err;
    }

    return 0;

err:
    os_mbuf_free_chain(om);
    return rc;
}

void
nm_init(void)
{
    int rc;

    rc = nmgr_transport_init(&nm_ble_transport, nm_ble_out);
    assert(rc == 0);
}
