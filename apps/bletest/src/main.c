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
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_cputime.h"
#include "hal/hal_flash.h"
#include "console/console.h"
#include "shell/shell.h"
#include "stats/stats.h"
#include "hal/flash_map.h"
#include "fs/fs.h"
#include "nffs/nffs.h"
#include "newtmgr/newtmgr.h"
#include "imgmgr/imgmgr.h"

/* BLE */
#include "nimble/ble.h"
#include "nimble/hci_transport.h"
#include "nimble/hci_common.h"
#include "host/host_hci.h"
#include "host/ble_hs.h"
#include "controller/ble_ll.h"
#include "controller/ble_ll_hci.h"
#include "controller/ble_ll_conn.h"
#include "controller/ble_ll_scan.h"
#include "controller/ble_ll_adv.h"

/* Task priorities */
#define BLE_LL_TASK_PRI     (OS_TASK_PRI_HIGHEST)
#define HOST_TASK_PRIO      (OS_TASK_PRI_HIGHEST + 1)
#define BLETEST_TASK_PRIO   (HOST_TASK_PRIO + 1)
#define SHELL_TASK_PRIO     (BLETEST_TASK_PRIO + 1)
#define NEWTMGR_TASK_PRIO   (SHELL_TASK_PRIO + 1)

/* Shell task stack */
#define SHELL_TASK_STACK_SIZE (OS_STACK_ALIGN(256))
os_stack_t shell_stack[SHELL_TASK_STACK_SIZE];

/* Newt manager task stack */
#define NEWTMGR_TASK_STACK_SIZE (OS_STACK_ALIGN(448))
os_stack_t newtmgr_stack[NEWTMGR_TASK_STACK_SIZE];

/* Shell maximum input line length */
#define SHELL_MAX_INPUT_LEN     (256)

/* For LED toggling */
int g_led_pin;

/* Our global device address (public) */
uint8_t g_dev_addr[BLE_DEV_ADDR_LEN];

/* Our random address (in case we need it) */
uint8_t g_random_addr[BLE_DEV_ADDR_LEN];

/* A buffer for host advertising data */
uint8_t g_host_adv_data[BLE_HCI_MAX_ADV_DATA_LEN];
uint8_t g_host_adv_len;

/* Create a mbuf pool of BLE mbufs */
#define MBUF_NUM_MBUFS      (32)
#define MBUF_BUF_SIZE       OS_ALIGN(BLE_MBUF_PAYLOAD_SIZE, 4)
#define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + BLE_MBUF_MEMBLOCK_OVERHEAD)
#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

struct os_mbuf_pool g_mbuf_pool;
struct os_mempool g_mbuf_mempool;
os_membuf_t g_mbuf_buffer[MBUF_MEMPOOL_SIZE];

/* Some application configurations */
#define BLETEST_ROLE_ADVERTISER         (0)
#define BLETEST_ROLE_SCANNER            (1)
#define BLETEST_ROLE_INITIATOR          (2)
#define BLETEST_CFG_ROLE                (BLETEST_ROLE_INITIATOR)
//#define BLETEST_CFG_ROLE                (BLETEST_ROLE_ADVERTISER)
//#define BLETEST_CFG_ROLE                (BLETEST_ROLE_SCANNER)
#define BLETEST_CFG_ADV_OWN_ADDR_TYPE   (BLE_HCI_ADV_OWN_ADDR_PUBLIC)
#define BLETEST_CFG_ADV_PEER_ADDR_TYPE  (BLE_HCI_ADV_PEER_ADDR_PUBLIC)
#define BLETEST_CFG_FILT_DUP_ADV        (0)
#define BLETEST_CFG_ADV_ITVL            (600000 / BLE_HCI_ADV_ITVL)
#define BLETEST_CFG_ADV_TYPE            BLE_HCI_ADV_TYPE_ADV_IND
#define BLETEST_CFG_ADV_FILT_POLICY     (BLE_HCI_ADV_FILT_NONE)
#define BLETEST_CFG_SCAN_ITVL           (700000 / BLE_HCI_SCAN_ITVL)
#define BLETEST_CFG_SCAN_WINDOW         (700000 / BLE_HCI_SCAN_ITVL)
#define BLETEST_CFG_SCAN_TYPE           (BLE_HCI_SCAN_TYPE_ACTIVE)
#define BLETEST_CFG_SCAN_FILT_POLICY    (BLE_HCI_SCAN_FILT_NO_WL)
#define BLETEST_CFG_CONN_ITVL           (128)  /* in 1.25 msec increments */
#define BLETEST_CFG_SLAVE_LATENCY       (0)
#define BLETEST_CFG_INIT_FILTER_POLICY  (BLE_HCI_CONN_FILT_NO_WL)
#define BLETEST_CFG_CONN_SPVN_TMO       (1000)  /* 20 seconds */
#define BLETEST_CFG_MIN_CE_LEN          (6)
#define BLETEST_CFG_MAX_CE_LEN          (BLETEST_CFG_CONN_ITVL)
#define BLETEST_CFG_CONN_PEER_ADDR_TYPE (BLE_HCI_CONN_PEER_ADDR_PUBLIC)
#define BLETEST_CFG_CONN_OWN_ADDR_TYPE  (BLE_HCI_ADV_OWN_ADDR_PUBLIC)
#define BLETEST_CFG_CONCURRENT_CONNS    (1)
#define BLETEST_CFG_RAND_PKT_SIZE       (1)
#define BLETEST_CFG_SUGG_DEF_TXOCTETS   (251)
#define BLETEST_CFG_SUGG_DEF_TXTIME     \
    BLE_TX_DUR_USECS_M(BLETEST_CFG_SUGG_DEF_TXOCTETS + 4)

/* Test configurations. One of these should be set to 1 */
#if !defined(BLETEST_CONCURRENT_CONN_TEST) && !defined(BLETEST_THROUGHPUT_TEST)
    #define BLETEST_CONCURRENT_CONN_TEST    (1)
#endif

/* BLETEST variables */
#undef BLETEST_ADV_PKT_NUM
#define BLETEST_MAX_PKT_SIZE            (247)
#define BLETEST_PKT_SIZE                (247)
#define BLETEST_STACK_SIZE              (256)
uint32_t g_next_os_time;
int g_bletest_state;
struct os_eventq g_bletest_evq;
struct os_callout_func g_bletest_timer;
struct os_task bletest_task;
bssnz_t os_stack_t bletest_stack[BLETEST_STACK_SIZE];
uint32_t g_bletest_conn_end;
int g_bletest_start_update;
uint32_t g_bletest_conn_upd_time;
uint8_t g_bletest_current_conns;
uint8_t g_bletest_cur_peer_addr[BLE_DEV_ADDR_LEN];
uint8_t g_last_handle_used;
uint8_t g_bletest_led_state;
uint32_t g_bletest_led_rate;
uint32_t g_bletest_next_led_time;
uint16_t g_bletest_handle;
uint16_t g_bletest_completed_pkts;
uint16_t g_bletest_outstanding_pkts;

/* --- For LE encryption testing --- */
/* Key: 0x4C68384139F574D836BCF34E9DFB01BF */
const uint8_t g_ble_ll_encrypt_test_key[16] =
{
    0x4c, 0x68, 0x38, 0x41, 0x39, 0xf5, 0x74, 0xd8,
    0x36, 0xbc, 0xf3, 0x4e, 0x9d, 0xfb, 0x01, 0xbf
};

/* Plaint text: 0x0213243546576879acbdcedfe0f10213 */
const uint8_t g_ble_ll_encrypt_test_plain_text[16] =
{
    0x02, 0x13, 0x24, 0x35, 0x46, 0x57, 0x68, 0x79,
    0xac, 0xbd, 0xce, 0xdf, 0xe0, 0xf1, 0x02, 0x13
};

/* Encrypted data: 0x99ad1b5226a37e3e058e3b8e27c2c666 */
const uint8_t g_ble_ll_encrypt_test_encrypted_data[16] =
{
    0x99, 0xad, 0x1b, 0x52, 0x26, 0xa3, 0x7e, 0x3e,
    0x05, 0x8e, 0x3b, 0x8e, 0x27, 0xc2, 0xc6, 0x66
};

uint8_t g_ble_ll_encrypted_data[16];

#if (BLETEST_THROUGHPUT_TEST == 1)
void
bletest_completed_pkt(uint16_t handle)
{
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    if (handle == g_bletest_handle) {
        ++g_bletest_completed_pkts;
    }
    OS_EXIT_CRITICAL(sr);
}
#endif

#ifdef BLETEST_ADV_PKT_NUM
void
bletest_inc_adv_pkt_num(void)
{
    int rc;
    uint8_t *dptr;
    uint8_t digit;

    if (g_host_adv_len != 0) {
        dptr = &g_host_adv_data[18];
        while (dptr >= &g_host_adv_data[13]) {
            digit = *dptr;
            ++digit;
            if (digit == 58) {
                digit = 48;
                *dptr = digit;
                --dptr;
            } else {
                *dptr = digit;
                break;
            }
        }

        rc = host_hci_cmd_le_set_adv_data(g_host_adv_data, g_host_adv_len);
        assert(rc == 0);
        host_hci_outstanding_opcode = 0;
    }
}
#endif

void
bletest_send_conn_update(uint16_t handle)
{
    int rc;
    struct hci_conn_update hcu;

    hcu.conn_latency = 4;
    hcu.supervision_timeout = 2000;
    hcu.conn_itvl_min = 1000;
    hcu.conn_itvl_max = 1000;
    hcu.handle = handle;
    hcu.min_ce_len = 4;
    hcu.max_ce_len = 4;

    rc = host_hci_cmd_le_conn_update(&hcu);
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;
}

/**
 * Sets the advertising data to be sent in advertising pdu's which contain
 * advertising data.
 *
 * @param dptr
 * @return uint8_t
 */
uint8_t
bletest_set_adv_data(uint8_t *dptr)
{
    uint8_t len;

    /* Place flags in first */
    dptr[0] = 2;
    dptr[1] = 0x01;     /* Flags identifier */
    dptr[2] = 0x06;
    dptr += 3;
    len = 3;

    /* Add local name */
    dptr[0] = 15;   /* Length of this data, not including the length */
    dptr[1] = 0x09;
    dptr[2] = 'r';
    dptr[3] = 'u';
    dptr[4] = 'n';
    dptr[5] = 't';
    dptr[6] = 'i';
    dptr[7] = 'm';
    dptr[8] = 'e';
    dptr[9] = '-';
    dptr[10] = '0';
    dptr[11] = '0';
    dptr[12] = '0';
    dptr[13] = '0';
    dptr[14] = '0';
    dptr[15] = '0';
    dptr += 16;
    len += 16;

    /* Add local device address */
    dptr[0] = 0x08;
    dptr[1] = 0x1B;
    dptr[2] = 0x00;
    memcpy(dptr + 3, g_dev_addr, BLE_DEV_ADDR_LEN);
    len += 9;

    g_host_adv_len = len;

    return len;
}

#if (BLETEST_CFG_ROLE == BLETEST_ROLE_ADVERTISER)
void
bletest_init_advertising(void)
{
    int rc;
    uint8_t adv_len;
    uint8_t rand_addr[BLE_DEV_ADDR_LEN];
    struct hci_adv_params adv;

    /* Just zero out advertising */
    memset(&adv, 0, sizeof(struct hci_adv_params));

    /* Set advertising parameters */
    adv.adv_type = BLETEST_CFG_ADV_TYPE;
    adv.adv_channel_map = 0x07;
    adv.adv_filter_policy = BLETEST_CFG_ADV_FILT_POLICY;
    adv.own_addr_type = BLETEST_CFG_ADV_OWN_ADDR_TYPE;
    adv.peer_addr_type = BLETEST_CFG_ADV_PEER_ADDR_TYPE;
    if ((adv.adv_type == BLE_HCI_ADV_TYPE_ADV_DIRECT_IND_HD) ||
        (adv.adv_type == BLE_HCI_ADV_TYPE_ADV_DIRECT_IND_LD)) {
        memcpy(adv.peer_addr, g_bletest_cur_peer_addr, BLE_DEV_ADDR_LEN);
        if (adv.peer_addr_type == BLE_HCI_ADV_PEER_ADDR_RANDOM) {
            adv.peer_addr[5] |= 0xc0;
        }
        adv_len = 0;
    } else {
        adv_len = bletest_set_adv_data(&g_host_adv_data[0]);
    }

    console_printf("Trying to connect to %x.%x.%x.%x.%x.%x\n",
                   adv.peer_addr[0], adv.peer_addr[1], adv.peer_addr[2],
                   adv.peer_addr[3], adv.peer_addr[4], adv.peer_addr[5]);

    if (adv.adv_type == BLE_HCI_ADV_TYPE_ADV_DIRECT_IND_HD) {
        adv.adv_itvl_min = 0;
        adv.adv_itvl_max = 0;
    } else {
        adv.adv_itvl_min = BLETEST_CFG_ADV_ITVL;
        adv.adv_itvl_max = BLETEST_CFG_ADV_ITVL; /* Advertising interval */
    }

    /* Set the advertising parameters */
    rc = host_hci_cmd_le_set_adv_params(&adv);
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;

    /* If we are using a random address, we need to set it */
    if (adv.own_addr_type == BLE_HCI_ADV_OWN_ADDR_RANDOM) {
        memcpy(rand_addr, g_dev_addr, BLE_DEV_ADDR_LEN);
        rand_addr[5] |= 0xc0;
        rc = host_hci_cmd_le_set_rand_addr(rand_addr);
        assert(rc == 0);
        host_hci_outstanding_opcode = 0;
    }

    /* Set advertising data */
    if (adv_len != 0) {
        rc = host_hci_cmd_le_set_adv_data(&g_host_adv_data[0], adv_len);
        assert(rc == 0);
        host_hci_outstanding_opcode = 0;

        /* Set scan response data */
        rc = host_hci_cmd_le_set_scan_rsp_data(&g_host_adv_data[0], adv_len);
        assert(rc == 0);
        host_hci_outstanding_opcode = 0;
    }
}
#endif

#if (BLETEST_CFG_ROLE == BLETEST_ROLE_SCANNER)
void
bletest_init_scanner(void)
{
    int rc;
    uint8_t dev_addr[BLE_DEV_ADDR_LEN];
    uint8_t filter_policy;

    /* Set scanning parameters */
    rc = host_hci_cmd_le_set_scan_params(BLETEST_CFG_SCAN_TYPE,
                                         BLETEST_CFG_SCAN_ITVL,
                                         BLETEST_CFG_SCAN_WINDOW,
                                         BLE_HCI_ADV_OWN_ADDR_PUBLIC,
                                         BLETEST_CFG_SCAN_FILT_POLICY);
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;

    filter_policy = BLETEST_CFG_SCAN_FILT_POLICY;
    if (filter_policy & 1) {
        /* Add some whitelist addresses */
        dev_addr[0] = 0x00;
        dev_addr[1] = 0x00;
        dev_addr[2] = 0x00;
        dev_addr[3] = 0x88;
        dev_addr[4] = 0x88;
        dev_addr[5] = 0x08;
        rc = host_hci_cmd_le_add_to_whitelist(dev_addr, BLE_ADDR_TYPE_PUBLIC);
        assert(rc == 0);
        host_hci_outstanding_opcode = 0;
    }
}

void
bletest_execute_scanner(void)
{
    int rc;

    /* Enable scanning */
    if ((int32_t)(os_time_get() - g_next_os_time) >= 0) {
        if (g_bletest_state) {
            rc = host_hci_cmd_le_set_scan_enable(0, BLETEST_CFG_FILT_DUP_ADV);
            assert(rc == 0);
            host_hci_outstanding_opcode = 0;
            g_bletest_state = 0;
        } else {
            rc = host_hci_cmd_le_set_scan_enable(1, BLETEST_CFG_FILT_DUP_ADV);
            assert(rc == 0);
            host_hci_outstanding_opcode = 0;
            g_bletest_state = 1;
        }
        g_next_os_time += (OS_TICKS_PER_SEC * 60);
    }
}
#endif

#if (BLETEST_CFG_ROLE == BLETEST_ROLE_INITIATOR)
void
bletest_init_initiator(void)
{
    int rc;
    uint8_t rand_addr[BLE_DEV_ADDR_LEN];
    struct hci_create_conn cc;
    struct hci_create_conn *hcc;

    /* Enable initiating */
    hcc = &cc;
    hcc->conn_itvl_max = BLETEST_CFG_CONN_ITVL;
    hcc->conn_itvl_min = BLETEST_CFG_CONN_ITVL;
    hcc->conn_latency = BLETEST_CFG_SLAVE_LATENCY;
    hcc->filter_policy = BLETEST_CFG_INIT_FILTER_POLICY;
    hcc->supervision_timeout = BLETEST_CFG_CONN_SPVN_TMO;
    hcc->scan_itvl = BLETEST_CFG_SCAN_ITVL;
    hcc->scan_window = BLETEST_CFG_SCAN_WINDOW;
    hcc->peer_addr_type = BLETEST_CFG_CONN_PEER_ADDR_TYPE;
    memcpy(hcc->peer_addr, g_bletest_cur_peer_addr, BLE_DEV_ADDR_LEN);
    if (hcc->peer_addr_type == BLE_HCI_CONN_PEER_ADDR_RANDOM) {
        hcc->peer_addr[5] |= 0xc0;
    }
    hcc->own_addr_type = BLETEST_CFG_CONN_OWN_ADDR_TYPE;
    hcc->min_ce_len = BLETEST_CFG_MIN_CE_LEN;
    hcc->max_ce_len = BLETEST_CFG_MAX_CE_LEN;

    console_printf("Trying to connect to %x.%x.%x.%x.%x.%x\n",
                   hcc->peer_addr[0], hcc->peer_addr[1], hcc->peer_addr[2],
                   hcc->peer_addr[3], hcc->peer_addr[4], hcc->peer_addr[5]);

    /* If we are using a random address, we need to set it */
    if (hcc->own_addr_type == BLE_HCI_ADV_OWN_ADDR_RANDOM) {
        memcpy(rand_addr, g_dev_addr, BLE_DEV_ADDR_LEN);
        rand_addr[5] |= 0xc0;
        rc = host_hci_cmd_le_set_rand_addr(rand_addr);
        assert(rc == 0);
        host_hci_outstanding_opcode = 0;
    }

    rc = host_hci_cmd_le_create_connection(hcc);
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;
}

void
bletest_execute_initiator(void)
{
    int i;
    int rc;
    uint16_t handle;
    uint8_t new_chan_map[5];

    /*
     * Determine if there is an active connection for the current handle
     * we are trying to create. If so, start looking for the next one
     */
    if (g_bletest_current_conns < BLETEST_CFG_CONCURRENT_CONNS) {
        handle = g_bletest_current_conns + 1;
        if (ble_ll_conn_find_active_conn(handle)) {
            /* Set LED to slower blink rate */
            g_bletest_led_rate = OS_TICKS_PER_SEC;

            /* Ask for version information */
            rc = host_hci_cmd_rd_rem_version(handle);
            host_hci_outstanding_opcode = 0;

            /* Ask for remote used features */
            rc = host_hci_cmd_le_read_rem_used_feat(handle);
            host_hci_outstanding_opcode = 0;

            /* Scanning better be stopped! */
            assert(ble_ll_scan_enabled() == 0);

            /* Add to current connections */
            if (!rc) {
                ++g_bletest_current_conns;

                /* Move to next connection */
                if (g_bletest_current_conns < BLETEST_CFG_CONCURRENT_CONNS) {
                    /* restart initiating */
                    g_bletest_cur_peer_addr[5] += 1;
                    g_dev_addr[5] += 1;
                    bletest_init_initiator();
                }
            }
        }
    } else {
        if ((int32_t)(os_time_get() - g_next_os_time) >= 0) {
            if ((g_bletest_state == 1) || (g_bletest_state == 3)) {
                for (i = 0; i < g_bletest_current_conns; ++i) {
                    if (ble_ll_conn_find_active_conn(i + 1)) {
                        host_hci_cmd_le_rd_chanmap(i+1);
                        host_hci_outstanding_opcode = 0;
                    }
                }
            } else if (g_bletest_state == 2) {
                new_chan_map[0] = 0;
                new_chan_map[1] = 0x3;
                new_chan_map[2] = 0;
                new_chan_map[3] = 0;
                new_chan_map[4] = 0;
                host_hci_cmd_le_set_host_chan_class(new_chan_map);
                host_hci_outstanding_opcode = 0;
            } else {
                for (i = 0; i < g_bletest_current_conns; ++i) {
                    if (ble_ll_conn_find_active_conn(i + 1)) {
                        host_hci_cmd_read_rssi(i+1);
                        host_hci_outstanding_opcode = 0;
                    }
                }
            }
            ++g_bletest_state;
            g_next_os_time = os_time_get() + OS_TICKS_PER_SEC * 5;
        }
    }
}
#endif

#if (BLETEST_CFG_ROLE == BLETEST_ROLE_ADVERTISER)
/*
 * Test wrapper to get packets. Only get a packet if we have more than half
 * left
 */
static struct os_mbuf *
bletest_get_packet(void)
{
    struct os_mbuf *om;

    om = NULL;
    if (g_mbuf_pool.omp_pool->mp_num_free >= 5) {
        om = os_msys_get_pkthdr(BLE_MBUF_PAYLOAD_SIZE,
                                sizeof(struct ble_mbuf_hdr));
    }
    return om;
}

static void
bletest_execute_advertiser(void)
{
    int i;
#if (BLETEST_CONCURRENT_CONN_TEST == 1)
    int j;
#endif
    int rc;
    uint16_t handle;
    uint16_t pktlen;
    struct os_mbuf *om;
#if (BLETEST_THROUGHPUT_TEST == 1)
    os_sr_t sr;
    uint16_t completed_pkts;
#endif

    /* See if we should start advertising again */
    if (g_bletest_current_conns < BLETEST_CFG_CONCURRENT_CONNS) {
        handle = g_bletest_current_conns + 1;
        if (ble_ll_conn_find_active_conn(handle)) {
            /* Set LED to slower blink rate */
            g_bletest_led_rate = OS_TICKS_PER_SEC;

#if (BLETEST_THROUGHPUT_TEST == 1)
            /* Set next os time to 10 seconds after 1st connection */
            if (g_next_os_time == 0) {
                g_next_os_time = os_time_get() + (10 * OS_TICKS_PER_SEC);
                g_bletest_handle = handle;
            }
#endif

            /* advertising better be stopped! */
            assert(ble_ll_adv_enabled() == 0);

            /* Send the remote used features command */
            rc = host_hci_cmd_le_read_rem_used_feat(handle);
            host_hci_outstanding_opcode = 0;
            if (rc) {
                return;
            }

            /* Send the remote read version command */
            rc = host_hci_cmd_rd_rem_version(handle);
            host_hci_outstanding_opcode = 0;
            if (rc) {
                return;
            }

            /* set conn update time */
            g_bletest_conn_upd_time = os_time_get() + (OS_TICKS_PER_SEC * 5);
            g_bletest_start_update = 1;

            /* Add to current connections */
            ++g_bletest_current_conns;

            /* Move to next connection */
            if (g_bletest_current_conns < BLETEST_CFG_CONCURRENT_CONNS) {
                /* restart initiating */
                g_bletest_cur_peer_addr[5] += 1;
                g_dev_addr[5] += 1;
                bletest_init_advertising();
                rc = host_hci_cmd_le_set_adv_enable(1);
                host_hci_outstanding_opcode = 0;
            }
        }
    }
#if 0
    if (g_bletest_start_update) {
        if ((int32_t)(os_time_get() - g_bletest_conn_upd_time) >= 0) {
            bletest_send_conn_update(1);
            g_bletest_start_update = 0;
        }
    }
#endif

#if (BLETEST_CONCURRENT_CONN_TEST == 1)
    /* See if it is time to hand a data packet to the connection */
    if ((int32_t)(os_time_get() - g_next_os_time) >= 0) {
        if (g_bletest_current_conns) {
            for (i = 0; i < g_bletest_current_conns; ++i) {
                if ((g_last_handle_used == 0) ||
                    (g_last_handle_used > g_bletest_current_conns)) {
                    g_last_handle_used = 1;
                }
                handle = g_last_handle_used;
                if (ble_ll_conn_find_active_conn(handle)) {
                    om = bletest_get_packet();
                    if (om) {
                        /* set payload length */
#if (BLETEST_CFG_RAND_PKT_SIZE == 1)
                        pktlen = rand() % (BLETEST_MAX_PKT_SIZE + 1);
#else
                        pktlen = BLETEST_PKT_SIZE;

#endif
                        /* Add header to length */
                        om->om_len = pktlen + 4;

                        /* Put the HCI header in the mbuf */
                        htole16(om->om_data, handle);
                        htole16(om->om_data + 2, om->om_len);

                        /* Place L2CAP header in packet */
                        htole16(om->om_data + 4, pktlen);
                        om->om_data[6] = 0;
                        om->om_data[7] = 0;
                        om->om_len += 4;

                        /* Fill with incrementing pattern (starting from 1) */
                        for (j = 0; j < pktlen; ++j) {
                            om->om_data[8 + j] = (uint8_t)(j + 1);
                        }

                        /* Add length */
                        OS_MBUF_PKTHDR(om)->omp_len = om->om_len;
                        ble_hci_transport_host_acl_data_send(om);

                        /* Increment last handle used */
                        ++g_last_handle_used;
                    }
                } else {
                    ++g_last_handle_used;
                }
            }
        }
        g_next_os_time = os_time_get() + OS_TICKS_PER_SEC;
    }
#endif

#if (BLETEST_THROUGHPUT_TEST == 1)
    /* Nothing to do if no connections */
    if (!g_bletest_current_conns) {
        return;
    }

    /* See if it is time to start throughput testing */
    if ((int32_t)(os_time_get() - g_next_os_time) >= 0) {

        /* Keep window full */
        OS_ENTER_CRITICAL(sr);
        completed_pkts = g_bletest_completed_pkts;
        g_bletest_completed_pkts = 0;
        OS_EXIT_CRITICAL(sr);

        assert(g_bletest_outstanding_pkts >= completed_pkts);
        g_bletest_outstanding_pkts -= completed_pkts;

        while (g_bletest_outstanding_pkts < 20) {
            om = bletest_get_packet();
            if (om) {
                /* set payload length */
                pktlen = BLETEST_PKT_SIZE;
                om->om_len = BLETEST_PKT_SIZE + 4;

                /* Put the HCI header in the mbuf */
                htole16(om->om_data, g_bletest_handle);
                htole16(om->om_data + 2, om->om_len);

                /* Place L2CAP header in packet */
                htole16(om->om_data + 4, pktlen);
                om->om_data[6] = 0;
                om->om_data[7] = 0;
                om->om_len += 4;

                /* Fill with incrementing pattern (starting from 1) */
                for (i = 0; i < pktlen; ++i) {
                    om->om_data[8 + i] = (uint8_t)(i + 1);
                }

                /* Add length */
                OS_MBUF_PKTHDR(om)->omp_len = om->om_len;
                ble_hci_transport_host_acl_data_send(om);

                ++g_bletest_outstanding_pkts;
            }
        }
    }
#endif /* XXX: throughput test */
}
#endif

/**
 * Main bletest function. Called by the task timer every 50 msecs.
 *
 */
void
bletest_execute(void)
{
    /* Toggle LED at set rate */
    if ((int32_t)(os_time_get() - g_bletest_next_led_time) >= 0) {
        hal_gpio_toggle(LED_BLINK_PIN);
        g_bletest_next_led_time = os_time_get() + g_bletest_led_rate;
    }
#if (BLETEST_CFG_ROLE == BLETEST_ROLE_ADVERTISER)
    bletest_execute_advertiser();
#endif
#if (BLETEST_CFG_ROLE == BLETEST_ROLE_SCANNER)
    bletest_execute_scanner();
#endif
#if (BLETEST_CFG_ROLE == BLETEST_ROLE_INITIATOR)
    bletest_execute_initiator();
#endif
}

/**
 * Callback when BLE test timer expires.
 *
 * @param arg
 */
void
bletest_timer_cb(void *arg)
{
    /* Call the bletest code */
    bletest_execute();

    /* Re-start the timer (run every 10 msecs) */
    os_callout_reset(&g_bletest_timer.cf_c, OS_TICKS_PER_SEC / 100);
}

/**
 * BLE test task
 *
 * @param arg
 */
void
bletest_task_handler(void *arg)
{
    int rc;
    uint64_t event_mask;
    struct os_event *ev;
    struct os_callout_func *cf;

    /* Set LED blink rate */
    g_bletest_led_rate = OS_TICKS_PER_SEC / 20;

    /* Wait one second before starting test task */
    os_time_delay(OS_TICKS_PER_SEC);

    /* Initialize eventq */
    os_eventq_init(&g_bletest_evq);

    /* Initialize the host timer */
    os_callout_func_init(&g_bletest_timer, &g_bletest_evq, bletest_timer_cb,
                         NULL);

    /* Send the reset command first */
    rc = host_hci_cmd_send(BLE_HCI_OGF_CTLR_BASEBAND, BLE_HCI_OCF_CB_RESET,
                           0, NULL);
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;

#if (BLETEST_CFG_ROLE == BLETEST_ROLE_ADVERTISER)
    /* Initialize the advertiser */
    console_printf("Starting BLE test task as advertiser\n");
    bletest_init_advertising();
#endif

#if (BLETEST_CFG_ROLE == BLETEST_ROLE_SCANNER)
    /* Initialize the scanner */
    console_printf("Starting BLE test task as scanner\n");
    bletest_init_scanner();
#endif

#if (BLETEST_CFG_ROLE == BLETEST_ROLE_INITIATOR)
    /* Initialize the scanner */
    console_printf("Starting BLE test task as initiator\n");
    bletest_init_initiator();
#endif

    /* Set the event mask we want to display */
    event_mask = 0x7FF;
    rc = host_hci_cmd_le_set_event_mask(event_mask);
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;

    /* Turn on all events */
    event_mask = 0xffffffffffffffff;
    rc = host_hci_cmd_set_event_mask(event_mask);
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;

    /* Read device address */
    rc = host_hci_cmd_rd_bd_addr();
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;

    /* Read local features */
    rc = host_hci_cmd_rd_local_feat();
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;

    /* Read local commands */
    rc = host_hci_cmd_rd_local_cmd();
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;

    /* Read version */
    rc = host_hci_cmd_rd_local_version();
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;

    /* Read supported states */
    rc = host_hci_cmd_le_read_supp_states();
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;

    /* Read maximum data length */
    rc = host_hci_cmd_le_read_max_datalen();
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;

    /* Read suggested data length */
    rc = host_hci_cmd_le_read_sugg_datalen();
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;

    /* write suggested default data length */
    rc = host_hci_cmd_le_write_sugg_datalen(BLETEST_CFG_SUGG_DEF_TXOCTETS,
                                            BLETEST_CFG_SUGG_DEF_TXTIME);
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;

    /* Read suggested data length */
    rc = host_hci_cmd_le_read_sugg_datalen();
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;

    /* Set data length (note: we know there is no connection; just a test) */
    rc = host_hci_cmd_le_set_datalen(0x1234,
                                     BLETEST_CFG_SUGG_DEF_TXOCTETS,
                                     BLETEST_CFG_SUGG_DEF_TXTIME);
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;


    /* Encrypt a block */
    rc = host_hci_cmd_le_encrypt((uint8_t *)g_ble_ll_encrypt_test_key,
                                 (uint8_t *)g_ble_ll_encrypt_test_plain_text);
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;

    /* Get a random number */
    rc = host_hci_cmd_le_rand();
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;

    /* Wait some time before starting */
    os_time_delay(OS_TICKS_PER_SEC);

    /* Init state */
    g_bletest_state = 0;

    /* Begin advertising if we are an advertiser */
#if (BLETEST_CFG_ROLE == BLETEST_ROLE_ADVERTISER)
    rc = host_hci_cmd_le_set_adv_enable(1);
    assert(rc == 0);
    host_hci_outstanding_opcode = 0;
#endif

    bletest_timer_cb(NULL);

    while (1) {
        ev = os_eventq_get(&g_bletest_evq);
        switch (ev->ev_type) {
        case OS_EVENT_T_TIMER:
            cf = (struct os_callout_func *)ev;
            assert(cf->cf_func);
            cf->cf_func(cf->cf_arg);
            break;
        default:
            assert(0);
            break;
        }
    }
}

/**
 * init_tasks
 *
 * Called by main.c after os_init(). This function performs initializations
 * that are required before tasks are running.
 *
 * @return int 0 success; error otherwise.
 */
static int
init_tasks(void)
{
    int rc;

    os_task_init(&bletest_task, "bletest", bletest_task_handler, NULL,
                 BLETEST_TASK_PRIO, OS_WAIT_FOREVER, bletest_stack,
                 BLETEST_STACK_SIZE);

    /* Initialize host HCI */
    rc = ble_hs_init(HOST_TASK_PRIO, NULL);
    assert(rc == 0);

    /* Initialize the BLE LL */
    rc = ble_ll_init(BLE_LL_TASK_PRI, MBUF_NUM_MBUFS, BLE_MBUF_PAYLOAD_SIZE);
    assert(rc == 0);

    return 0;
}

/**
 * main
 *
 * The main function for the project. This function initializes the os, calls
 * init_tasks to initialize tasks (and possibly other objects), then starts the
 * OS. We should not return from os start.
 *
 * @return int NOTE: this function should never return!
 */
int
main(void)
{
    int i;
    int rc;
    uint32_t seed;
#if 0
    int cnt;
    struct nffs_area_desc descs[NFFS_AREA_MAX];
#endif

    /* Initialize OS */
    os_init();

    /* Set cputime to count at 1 usec increments */
    rc = cputime_init(1000000);
    assert(rc == 0);

    rc = os_mempool_init(&g_mbuf_mempool, MBUF_NUM_MBUFS,
            MBUF_MEMBLOCK_SIZE, &g_mbuf_buffer[0], "mbuf_pool");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&g_mbuf_pool, &g_mbuf_mempool, MBUF_MEMBLOCK_SIZE,
                           MBUF_NUM_MBUFS);
    assert(rc == 0);

    rc = os_msys_register(&g_mbuf_pool);
    assert(rc == 0);

    /* Dummy device address */
#if BLETEST_CFG_ROLE == BLETEST_ROLE_ADVERTISER
    g_dev_addr[0] = 0x00;
    g_dev_addr[1] = 0x00;
    g_dev_addr[2] = 0x00;
    g_dev_addr[3] = 0x88;
    g_dev_addr[4] = 0x88;
    g_dev_addr[5] = 0x08;

    g_bletest_cur_peer_addr[0] = 0x00;
    g_bletest_cur_peer_addr[1] = 0x00;
    g_bletest_cur_peer_addr[2] = 0x00;
    g_bletest_cur_peer_addr[3] = 0x99;
    g_bletest_cur_peer_addr[4] = 0x99;
    g_bletest_cur_peer_addr[5] = 0x09;
#else
    g_dev_addr[0] = 0x00;
    g_dev_addr[1] = 0x00;
    g_dev_addr[2] = 0x00;
    g_dev_addr[3] = 0x99;
    g_dev_addr[4] = 0x99;
    g_dev_addr[5] = 0x09;

    g_bletest_cur_peer_addr[0] = 0x00;
    g_bletest_cur_peer_addr[1] = 0x00;
    g_bletest_cur_peer_addr[2] = 0x00;
    g_bletest_cur_peer_addr[3] = 0x88;
    g_bletest_cur_peer_addr[4] = 0x88;
    g_bletest_cur_peer_addr[5] = 0x08;
#endif

    /*
     * Seed random number generator with least significant bytes of device
     * address.
     */
    seed = 0;
    for (i = 0; i < 4; ++i) {
        seed |= g_dev_addr[i];
        seed <<= 8;
    }
    srand(seed);

    /* Set the led pin as an output */
    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);

    /* Init the console */
    rc = console_init(shell_console_rx_cb);
    assert(rc == 0);

#if 0
    rc = hal_flash_init();
    assert(rc == 0);

    nffs_config.nc_num_inodes = 32;
    nffs_config.nc_num_blocks = 64;
    nffs_config.nc_num_files = 2;
    nffs_config.nc_num_dirs = 2;
    rc = nffs_init();
    assert(rc == 0);

    cnt = NFFS_AREA_MAX;
    rc = flash_area_to_nffs_desc(FLASH_AREA_NFFS, &cnt, descs);
    assert(rc == 0);
    if (nffs_detect(descs) == FS_ECORRUPT) {
        rc = nffs_format(descs);
        assert(rc == 0);
    }
#endif

    shell_task_init(SHELL_TASK_PRIO, shell_stack, SHELL_TASK_STACK_SIZE,
                         SHELL_MAX_INPUT_LEN);

    nmgr_task_init(NEWTMGR_TASK_PRIO, newtmgr_stack, NEWTMGR_TASK_STACK_SIZE);
#if 0
    imgmgr_module_init();
#endif

    /* Init statistics module */
    stats_module_init();

    /* Init tasks */
    init_tasks();

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return rc;
}
