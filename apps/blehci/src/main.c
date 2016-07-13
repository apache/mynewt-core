/**
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
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "bsp/bsp.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_cputime.h"
#include "hal/hal_uart.h"

/* BLE */
#include "nimble/ble.h"
#include "nimble/nimble_opt.h"
#include "nimble/hci_transport.h"
#include "controller/ble_ll.h"

#define HCI_UART_SPEED 1000000
#define HCI_UART CONSOLE_UART

/* Nimble task priorities */
#define BLE_LL_TASK_PRI         (OS_TASK_PRI_HIGHEST)

/* Create a mbuf pool of BLE mbufs */
#define MBUF_NUM_MBUFS      (7)
#define MBUF_BUF_SIZE       OS_ALIGN(BLE_MBUF_PAYLOAD_SIZE, 4)
#define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + BLE_MBUF_MEMBLOCK_OVERHEAD)
#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

/* Our global device address (public) */
uint8_t g_dev_addr[BLE_DEV_ADDR_LEN] = { 0 };

/* Our random address (in case we need it) */
uint8_t g_random_addr[BLE_DEV_ADDR_LEN] = { 0 };

#define HCI_MAX_BUFS        (5)

#define HCI_EVT_BUF_SIZE    (260)
struct os_mempool g_hci_evt_pool;
static void *hci_cmd_buf;

#define HCI_OS_EVENT_BUF_SIZE   (sizeof(struct os_event))

#define BLE_HOST_HCI_EVENT_CTLR_EVENT   (OS_EVENT_T_PERUSER + 0)
#define BLE_HOST_HCI_EVENT_CTLR_DATA    (OS_EVENT_T_PERUSER + 1)

struct os_mempool g_hci_os_event_pool;
static void *hci_os_event_buf;

os_membuf_t default_mbuf_mpool_data[MBUF_MEMPOOL_SIZE];

struct os_mbuf_pool default_mbuf_pool;
struct os_mempool default_mbuf_mpool;

#define H4_NONE 0x00
#define H4_CMD  0x01
#define H4_ACL  0x02
#define H4_SCO  0x03
#define H4_EVT  0x04

#define HCI_CMD_HDR_LEN 3
#define HCI_ACL_HDR_LEN 4
#define HCI_EVT_HDR_LEN 2

struct memblock {
    uint8_t *data;      /* Pointer to memblock data */
    uint16_t cur;       /* Number of bytes read/written */
    uint16_t len;       /* Total number of bytes to read/write */
};

struct tx_acl {
    struct os_mbuf *buf; /* Buffer containing the data */
    uint16_t len;        /* Target size when buf is considered complete */
};

static struct {
    /* State of data from host to controller */
    uint8_t tx_type;    /* Pending packet type. 0 means nothing pending */
    union {
        struct memblock tx_cmd;
        struct tx_acl tx_acl;
    };

    /* State of data from controller to host */
    uint8_t rx_type;    /* Pending packet type. 0 means nothing pending */
    union {
        struct memblock rx_evt;
        struct os_mbuf *rx_acl;
    };
    STAILQ_HEAD(, os_event) rx_pkts; /* Packet queue to send to UART */
} hci;

int
ble_hs_rx_data(struct os_mbuf *om)
{
    struct os_event *ev;
    os_sr_t sr;

    ev = os_memblock_get(&g_hci_os_event_pool);
    if (!ev) {
        os_mbuf_free_chain(om);
        return -1;
    }

    ev->ev_type = BLE_HOST_HCI_EVENT_CTLR_DATA;
    ev->ev_arg = om;
    ev->ev_queued = 1;

    OS_ENTER_CRITICAL(sr);
    STAILQ_INSERT_TAIL(&hci.rx_pkts, ev, ev_next);
    OS_EXIT_CRITICAL(sr);

    hal_uart_start_tx(HCI_UART);

    return 0;
}

int
ble_hci_transport_ctlr_event_send(uint8_t *hci_ev)
{
    struct os_event *ev;
    os_sr_t sr;

    ev = os_memblock_get(&g_hci_os_event_pool);
    if (!ev) {
        os_error_t err;

        err = os_memblock_put(&g_hci_evt_pool, hci_ev);
        assert(err == OS_OK);

        return -1;
    }

    ev->ev_type = BLE_HOST_HCI_EVENT_CTLR_EVENT;
    ev->ev_arg = hci_ev;
    ev->ev_queued = 1;

    OS_ENTER_CRITICAL(sr);
    STAILQ_INSERT_TAIL(&hci.rx_pkts, ev, ev_next);
    OS_EXIT_CRITICAL(sr);

    hal_uart_start_tx(HCI_UART);

    return 0;
}

static int
uart_tx_pkt_type(void)
{
    struct os_event *ev;
    os_sr_t sr;
    int rc;

    OS_ENTER_CRITICAL(sr);

    ev = STAILQ_FIRST(&hci.rx_pkts);
    if (!ev) {
        OS_EXIT_CRITICAL(sr);
        return -1;
    }

    STAILQ_REMOVE(&hci.rx_pkts, ev, os_event, ev_next);
    ev->ev_queued = 0;

    OS_EXIT_CRITICAL(sr);

    switch (ev->ev_type) {
    case BLE_HOST_HCI_EVENT_CTLR_EVENT:
        hci.rx_type = H4_EVT;
        hci.rx_evt.data = ev->ev_arg;
        hci.rx_evt.cur = 0;
        hci.rx_evt.len = hci.rx_evt.data[1] + HCI_EVT_HDR_LEN;
        rc = H4_EVT;
        break;
    case BLE_HOST_HCI_EVENT_CTLR_DATA:
        hci.rx_type = H4_ACL;
        hci.rx_acl = ev->ev_arg;
        rc = H4_ACL;
        break;
    default:
        rc = -1;
        break;
    }

    os_memblock_put(&g_hci_os_event_pool, ev);

    return rc;
}

static int
uart_tx_char(void *arg)
{
    int rc = -1;

    switch (hci.rx_type) {
    case H4_NONE: /* No pending packet, pick one from the queue */
        rc = uart_tx_pkt_type();
        break;
    case H4_EVT:
        rc = hci.rx_evt.data[hci.rx_evt.cur++];

        if (hci.rx_evt.cur == hci.rx_evt.len) {
            os_memblock_put(&g_hci_evt_pool, hci.rx_evt.data);
            hci.rx_type = H4_NONE;
        }

        break;
    case H4_ACL:
        rc = *OS_MBUF_DATA(hci.rx_acl, uint8_t *);
        os_mbuf_adj(hci.rx_acl, 1);
        if (!OS_MBUF_PKTLEN(hci.rx_acl)) {
            os_mbuf_free_chain(hci.rx_acl);
            hci.rx_type = H4_NONE;
        }

        break;
    }

    return rc;
}

static int
uart_rx_pkt_type(uint8_t data)
{
    hci.tx_type = data;

    switch (hci.tx_type) {
    case H4_CMD:
        hci.tx_cmd.data = os_memblock_get(&g_hci_evt_pool);
        hci.tx_cmd.len = 0;
        hci.tx_cmd.cur = 0;
        break;
    case H4_ACL:
        hci.tx_acl.buf = os_msys_get_pkthdr(HCI_ACL_HDR_LEN, 0);
        hci.tx_acl.len = 0;
        break;
    default:
        hci.tx_type = H4_NONE;
        return -1;
    }

    return 0;
}

static int
uart_rx_cmd(uint8_t data)
{
    int rc;

    hci.tx_cmd.data[hci.tx_cmd.cur++] = data;

    if (hci.tx_cmd.cur < HCI_CMD_HDR_LEN) {
        return 0;
    } else if (hci.tx_cmd.cur == HCI_CMD_HDR_LEN) {
        hci.tx_cmd.len = hci.tx_cmd.data[2] + HCI_CMD_HDR_LEN;
    }

    if (hci.tx_cmd.cur == hci.tx_cmd.len) {
        rc = ble_hci_transport_host_cmd_send(hci.tx_cmd.data);
        if (rc != 0) {
            os_memblock_put(&g_hci_evt_pool, hci.tx_cmd.data);
        }
        hci.tx_type = H4_NONE;
    }

    return 0;
}

static int
uart_rx_acl(uint8_t data)
{
    os_mbuf_append(hci.tx_acl.buf, &data, 1);

    if (OS_MBUF_PKTLEN(hci.tx_acl.buf) < HCI_ACL_HDR_LEN) {
        return 0;
    } else if (OS_MBUF_PKTLEN(hci.tx_acl.buf) == HCI_ACL_HDR_LEN) {
        os_mbuf_copydata(hci.tx_acl.buf, 2, sizeof(hci.tx_acl.len),
                         &hci.tx_acl.len);
        hci.tx_acl.len = le16toh(&hci.tx_acl.len) + HCI_ACL_HDR_LEN;
    }

    if (OS_MBUF_PKTLEN(hci.tx_acl.buf) == hci.tx_acl.len) {
        ble_hci_transport_host_acl_data_send(hci.tx_acl.buf);
        hci.tx_type = H4_NONE;
    }

    return 0;
}

static int
uart_rx_char(void *arg, uint8_t data)
{
    switch (hci.tx_type) {
    case H4_NONE:
        return uart_rx_pkt_type(data);
    case H4_CMD:
        return uart_rx_cmd(data);
    case H4_ACL:
        return uart_rx_acl(data);
    default:
        return -1;
    }
}

static int
uart_init(void)
{
    int rc;

    memset(&hci, 0, sizeof(hci));

    STAILQ_INIT(&hci.rx_pkts);

    rc = hal_uart_init_cbs(HCI_UART, uart_tx_char, NULL, uart_rx_char, NULL);
    if (rc) {
        return rc;
    }

    return hal_uart_config(HCI_UART, HCI_UART_SPEED, 8, 1, HAL_UART_PARITY_NONE,
                           HAL_UART_FLOW_CTL_RTS_CTS);
}

int
main(void)
{
    int rc;

    /* Initialize OS */
    os_init();

    /* Set cputime to count at 1 usec increments */
    rc = cputime_init(1000000);
    assert(rc == 0);

    rc = os_mempool_init(&default_mbuf_mpool, MBUF_NUM_MBUFS,
                         MBUF_MEMBLOCK_SIZE, default_mbuf_mpool_data,
                         "default_mbuf_data");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&default_mbuf_pool, &default_mbuf_mpool,
                           MBUF_MEMBLOCK_SIZE, MBUF_NUM_MBUFS);
    assert(rc == 0);

    rc = os_msys_register(&default_mbuf_pool);
    assert(rc == 0);

    /* Initialize the BLE LL */
    rc = ble_ll_init(BLE_LL_TASK_PRI, MBUF_NUM_MBUFS, BLE_MBUF_PAYLOAD_SIZE);
    assert(rc == 0);

    hci_cmd_buf = malloc(OS_MEMPOOL_BYTES(HCI_MAX_BUFS, HCI_EVT_BUF_SIZE));
    assert(hci_cmd_buf != NULL);

    /* Create memory pool of command buffers */
    rc = os_mempool_init(&g_hci_evt_pool, HCI_MAX_BUFS, HCI_EVT_BUF_SIZE,
                         hci_cmd_buf, "HCICmdPool");
    assert(rc == 0);

    hci_os_event_buf = malloc(OS_MEMPOOL_BYTES(HCI_MAX_BUFS,
                              HCI_OS_EVENT_BUF_SIZE));
    assert(hci_os_event_buf != NULL);

    /* Create memory pool of OS events */
    rc = os_mempool_init(&g_hci_os_event_pool, HCI_MAX_BUFS,
                         HCI_OS_EVENT_BUF_SIZE, hci_os_event_buf,
                         "HCIOsEventPool");
    assert(rc == 0);

    rc = uart_init();
    assert(rc == 0);

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return 0;
}
