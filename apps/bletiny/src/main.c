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
#include "log/log.h"
#include "stats/stats.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_cputime.h"
#include "console/console.h"
#include "shell/shell.h"
#include "bletiny.h"

/* BLE */
#include "nimble/ble.h"
#include "nimble/nimble_opt.h"
#include "nimble/ble_hci_trans.h"
#include "controller/ble_ll.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_uuid.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_store.h"
#include "host/ble_sm.h"

/* RAM HCI transport. */
#include "transport/ram/ble_hci_ram.h"

/* RAM persistence layer. */
#include "store/ram/ble_store_ram.h"

/* Mandatory services. */
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

/* XXX: An app should not include private headers from a library.  The bletiny
 * app uses some of nimble's internal details for logging.
 */
#include "../src/ble_hs_conn_priv.h"
#include "../src/ble_hs_atomic_priv.h"
#include "../src/ble_hs_hci_priv.h"

/* Nimble task priorities */
#define BLE_LL_TASK_PRI         (OS_TASK_PRI_HIGHEST)

#define SHELL_TASK_PRIO         (3)
#define SHELL_MAX_INPUT_LEN     (256)
#define SHELL_TASK_STACK_SIZE   (OS_STACK_ALIGN(512))
static bssnz_t os_stack_t shell_stack[SHELL_TASK_STACK_SIZE];

/* Our global device address (public) */
uint8_t g_dev_addr[BLE_DEV_ADDR_LEN];

/* Our random address (in case we need it) */
uint8_t g_random_addr[BLE_DEV_ADDR_LEN];

/* A buffer for host advertising data */
uint8_t g_host_adv_len;

/** Our public address.  Note: this is in reverse byte order. */
static uint8_t bletiny_addr[6] = {0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a};

/* Create a mbuf pool of BLE mbufs */
#define MBUF_NUM_MBUFS      (16)
#define MBUF_BUF_SIZE       OS_ALIGN(BLE_MBUF_PAYLOAD_SIZE, 4)
#define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + BLE_MBUF_MEMBLOCK_OVERHEAD)
#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

os_membuf_t default_mbuf_mpool_data[MBUF_MEMPOOL_SIZE];

struct os_mbuf_pool default_mbuf_pool;
struct os_mempool default_mbuf_mpool;

/* BLETINY variables */
#define BLETINY_STACK_SIZE             (OS_STACK_ALIGN(512))
#define BLETINY_TASK_PRIO              1

#if NIMBLE_OPT(ROLE_CENTRAL)
#define BLETINY_MAX_SVCS               32
#define BLETINY_MAX_CHRS               64
#define BLETINY_MAX_DSCS               64
#else
#define BLETINY_MAX_SVCS               1
#define BLETINY_MAX_CHRS               1
#define BLETINY_MAX_DSCS               1
#endif

struct os_eventq bletiny_evq;
struct os_task bletiny_task;
bssnz_t os_stack_t bletiny_stack[BLETINY_STACK_SIZE];

static struct log_handler bletiny_log_console_handler;
struct log bletiny_log;

bssnz_t struct bletiny_conn bletiny_conns[NIMBLE_OPT(MAX_CONNECTIONS)];
int bletiny_num_conns;

static void *bletiny_svc_mem;
static struct os_mempool bletiny_svc_pool;

static void *bletiny_chr_mem;
static struct os_mempool bletiny_chr_pool;

static void *bletiny_dsc_mem;
static struct os_mempool bletiny_dsc_pool;

static struct os_callout_func bletiny_tx_timer;
struct bletiny_tx_data_s
{
    uint16_t tx_num;
    uint16_t tx_rate;
    uint16_t tx_handle;
    uint16_t tx_len;
};
static struct bletiny_tx_data_s bletiny_tx_data;
int bletiny_full_disc_prev_chr_val;

#define XSTR(s) STR(s)
#define STR(s) #s

#ifdef DEVICE_NAME
#define BLETINY_AUTO_DEVICE_NAME    XSTR(DEVICE_NAME)
#else
#define BLETINY_AUTO_DEVICE_NAME    ""
#endif

static void
bletiny_print_error(char *msg, uint16_t conn_handle,
                    const struct ble_gatt_error *error)
{
    if (msg == NULL) {
        msg = "ERROR";
    }

    console_printf("%s: conn_handle=%d status=%d att_handle=%d\n",
                   msg, conn_handle, error->status, error->att_handle);
}

static void
bletiny_print_adv_fields(const struct ble_hs_adv_fields *fields)
{
    uint8_t *u8p;
    int i;

    if (fields->flags_is_present) {
        console_printf("    flags=0x%02x\n", fields->flags);
    }

    if (fields->uuids16 != NULL) {
        console_printf("    uuids16(%scomplete)=",
                       fields->uuids16_is_complete ? "" : "in");
        for (i = 0; i < fields->num_uuids16; i++) {
            console_printf("0x%04x ", fields->uuids16[i]);
        }
        console_printf("\n");
    }

    if (fields->uuids32 != NULL) {
        console_printf("    uuids32(%scomplete)=",
                       fields->uuids32_is_complete ? "" : "in");
        for (i = 0; i < fields->num_uuids32; i++) {
            console_printf("0x%08x ", (unsigned int)fields->uuids32[i]);
        }
        console_printf("\n");
    }

    if (fields->uuids128 != NULL) {
        console_printf("    uuids128(%scomplete)=",
                       fields->uuids128_is_complete ? "" : "in");
        u8p = fields->uuids128;
        for (i = 0; i < fields->num_uuids128; i++) {
            print_uuid(u8p);
            console_printf(" ");
            u8p += 16;
        }
        console_printf("\n");
    }

    if (fields->name != NULL) {
        console_printf("    name(%scomplete)=",
                       fields->name_is_complete ? "" : "in");
        console_write((char *)fields->name, fields->name_len);
        console_printf("\n");
    }

    if (fields->tx_pwr_lvl_is_present) {
        console_printf("    tx_pwr_lvl=%d\n", fields->tx_pwr_lvl);
    }

    if (fields->device_class != NULL) {
        console_printf("    device_class=");
        print_bytes(fields->device_class,
                            BLE_HS_ADV_DEVICE_CLASS_LEN);
        console_printf("\n");
    }

    if (fields->slave_itvl_range != NULL) {
        console_printf("    slave_itvl_range=");
        print_bytes(fields->slave_itvl_range,
                            BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN);
        console_printf("\n");
    }

    if (fields->svc_data_uuid16 != NULL) {
        console_printf("    svc_data_uuid16=");
        print_bytes(fields->svc_data_uuid16,
                            fields->svc_data_uuid16_len);
        console_printf("\n");
    }

    if (fields->public_tgt_addr != NULL) {
        console_printf("    public_tgt_addr=");
        u8p = fields->public_tgt_addr;
        for (i = 0; i < fields->num_public_tgt_addrs; i++) {
            print_addr(u8p);
            u8p += BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN;
        }
        console_printf("\n");
    }

    if (fields->appearance_is_present) {
        console_printf("    appearance=0x%04x\n", fields->appearance);
    }

    if (fields->adv_itvl_is_present) {
        console_printf("    adv_itvl=0x%04x\n", fields->adv_itvl);
    }

    if (fields->le_addr != NULL) {
        console_printf("    le_addr=");
        print_addr(fields->le_addr);
        console_printf("\n");
    }

    if (fields->le_role_is_present) {
        console_printf("    le_role=0x%02x\n", fields->le_role);
    }

    if (fields->svc_data_uuid32 != NULL) {
        console_printf("    svc_data_uuid32=");
        print_bytes(fields->svc_data_uuid32,
                             fields->svc_data_uuid32_len);
        console_printf("\n");
    }

    if (fields->svc_data_uuid128 != NULL) {
        console_printf("    svc_data_uuid128=");
        print_bytes(fields->svc_data_uuid128,
                            fields->svc_data_uuid128_len);
        console_printf("\n");
    }

    if (fields->uri != NULL) {
        console_printf("    uri=");
        print_bytes(fields->uri, fields->uri_len);
        console_printf("\n");
    }

    if (fields->mfg_data != NULL) {
        console_printf("    mfg_data=");
        print_bytes(fields->mfg_data, fields->mfg_data_len);
        console_printf("\n");
    }
}

static int
bletiny_conn_find_idx(uint16_t handle)
{
    int i;

    for (i = 0; i < bletiny_num_conns; i++) {
        if (bletiny_conns[i].handle == handle) {
            return i;
        }
    }

    return -1;
}

static struct bletiny_conn *
bletiny_conn_find(uint16_t handle)
{
    int idx;

    idx = bletiny_conn_find_idx(handle);
    if (idx == -1) {
        return NULL;
    } else {
        return bletiny_conns + idx;
    }
}

static struct bletiny_svc *
bletiny_svc_find_prev(struct bletiny_conn *conn, uint16_t svc_start_handle)
{
    struct bletiny_svc *prev;
    struct bletiny_svc *svc;

    prev = NULL;
    SLIST_FOREACH(svc, &conn->svcs, next) {
        if (svc->svc.start_handle >= svc_start_handle) {
            break;
        }

        prev = svc;
    }

    return prev;
}

static struct bletiny_svc *
bletiny_svc_find(struct bletiny_conn *conn, uint16_t svc_start_handle,
                 struct bletiny_svc **out_prev)
{
    struct bletiny_svc *prev;
    struct bletiny_svc *svc;

    prev = bletiny_svc_find_prev(conn, svc_start_handle);
    if (prev == NULL) {
        svc = SLIST_FIRST(&conn->svcs);
    } else {
        svc = SLIST_NEXT(prev, next);
    }

    if (svc != NULL && svc->svc.start_handle != svc_start_handle) {
        svc = NULL;
    }

    if (out_prev != NULL) {
        *out_prev = prev;
    }
    return svc;
}

static struct bletiny_svc *
bletiny_svc_find_range(struct bletiny_conn *conn, uint16_t attr_handle)
{
    struct bletiny_svc *svc;

    SLIST_FOREACH(svc, &conn->svcs, next) {
        if (svc->svc.start_handle <= attr_handle &&
            svc->svc.end_handle >= attr_handle) {

            return svc;
        }
    }

    return NULL;
}

static void
bletiny_chr_delete(struct bletiny_chr *chr)
{
    struct bletiny_dsc *dsc;

    while ((dsc = SLIST_FIRST(&chr->dscs)) != NULL) {
        SLIST_REMOVE_HEAD(&chr->dscs, next);
        os_memblock_put(&bletiny_dsc_pool, dsc);
    }

    os_memblock_put(&bletiny_chr_pool, chr);
}

static void
bletiny_svc_delete(struct bletiny_svc *svc)
{
    struct bletiny_chr *chr;

    while ((chr = SLIST_FIRST(&svc->chrs)) != NULL) {
        SLIST_REMOVE_HEAD(&svc->chrs, next);
        bletiny_chr_delete(chr);
    }

    os_memblock_put(&bletiny_svc_pool, svc);
}

static struct bletiny_svc *
bletiny_svc_add(uint16_t conn_handle, const struct ble_gatt_svc *gatt_svc)
{
    struct bletiny_conn *conn;
    struct bletiny_svc *prev;
    struct bletiny_svc *svc;

    conn = bletiny_conn_find(conn_handle);
    if (conn == NULL) {
        BLETINY_LOG(DEBUG, "RECEIVED SERVICE FOR UNKNOWN CONNECTION; "
                           "HANDLE=%d\n",
                    conn_handle);
        return NULL;
    }

    svc = bletiny_svc_find(conn, gatt_svc->start_handle, &prev);
    if (svc != NULL) {
        /* Service already discovered. */
        return svc;
    }

    svc = os_memblock_get(&bletiny_svc_pool);
    if (svc == NULL) {
        BLETINY_LOG(DEBUG, "OOM WHILE DISCOVERING SERVICE\n");
        return NULL;
    }
    memset(svc, 0, sizeof *svc);

    svc->svc = *gatt_svc;
    SLIST_INIT(&svc->chrs);

    if (prev == NULL) {
        SLIST_INSERT_HEAD(&conn->svcs, svc, next);
    } else {
        SLIST_INSERT_AFTER(prev, svc, next);
    }

    return svc;
}

static struct bletiny_chr *
bletiny_chr_find_prev(const struct bletiny_svc *svc, uint16_t chr_val_handle)
{
    struct bletiny_chr *prev;
    struct bletiny_chr *chr;

    prev = NULL;
    SLIST_FOREACH(chr, &svc->chrs, next) {
        if (chr->chr.val_handle >= chr_val_handle) {
            break;
        }

        prev = chr;
    }

    return prev;
}

static struct bletiny_chr *
bletiny_chr_find(const struct bletiny_svc *svc, uint16_t chr_val_handle,
                 struct bletiny_chr **out_prev)
{
    struct bletiny_chr *prev;
    struct bletiny_chr *chr;

    prev = bletiny_chr_find_prev(svc, chr_val_handle);
    if (prev == NULL) {
        chr = SLIST_FIRST(&svc->chrs);
    } else {
        chr = SLIST_NEXT(prev, next);
    }

    if (chr != NULL && chr->chr.val_handle != chr_val_handle) {
        chr = NULL;
    }

    if (out_prev != NULL) {
        *out_prev = prev;
    }
    return chr;
}

static struct bletiny_chr *
bletiny_chr_add(uint16_t conn_handle,  uint16_t svc_start_handle,
                const struct ble_gatt_chr *gatt_chr)
{
    struct bletiny_conn *conn;
    struct bletiny_chr *prev;
    struct bletiny_chr *chr;
    struct bletiny_svc *svc;

    conn = bletiny_conn_find(conn_handle);
    if (conn == NULL) {
        BLETINY_LOG(DEBUG, "RECEIVED SERVICE FOR UNKNOWN CONNECTION; "
                           "HANDLE=%d\n",
                    conn_handle);
        return NULL;
    }

    svc = bletiny_svc_find(conn, svc_start_handle, NULL);
    if (svc == NULL) {
        BLETINY_LOG(DEBUG, "CAN'T FIND SERVICE FOR DISCOVERED CHR; HANDLE=%d\n",
                    conn_handle);
        return NULL;
    }

    chr = bletiny_chr_find(svc, gatt_chr->val_handle, &prev);
    if (chr != NULL) {
        /* Characteristic already discovered. */
        return chr;
    }

    chr = os_memblock_get(&bletiny_chr_pool);
    if (chr == NULL) {
        BLETINY_LOG(DEBUG, "OOM WHILE DISCOVERING CHARACTERISTIC\n");
        return NULL;
    }
    memset(chr, 0, sizeof *chr);

    chr->chr = *gatt_chr;

    if (prev == NULL) {
        SLIST_INSERT_HEAD(&svc->chrs, chr, next);
    } else {
        SLIST_NEXT(prev, next) = chr;
    }

    return chr;
}

static struct bletiny_dsc *
bletiny_dsc_find_prev(const struct bletiny_chr *chr, uint16_t dsc_handle)
{
    struct bletiny_dsc *prev;
    struct bletiny_dsc *dsc;

    prev = NULL;
    SLIST_FOREACH(dsc, &chr->dscs, next) {
        if (dsc->dsc.handle >= dsc_handle) {
            break;
        }

        prev = dsc;
    }

    return prev;
}

static struct bletiny_dsc *
bletiny_dsc_find(const struct bletiny_chr *chr, uint16_t dsc_handle,
                 struct bletiny_dsc **out_prev)
{
    struct bletiny_dsc *prev;
    struct bletiny_dsc *dsc;

    prev = bletiny_dsc_find_prev(chr, dsc_handle);
    if (prev == NULL) {
        dsc = SLIST_FIRST(&chr->dscs);
    } else {
        dsc = SLIST_NEXT(prev, next);
    }

    if (dsc != NULL && dsc->dsc.handle != dsc_handle) {
        dsc = NULL;
    }

    if (out_prev != NULL) {
        *out_prev = prev;
    }
    return dsc;
}

static struct bletiny_dsc *
bletiny_dsc_add(uint16_t conn_handle, uint16_t chr_val_handle,
                const struct ble_gatt_dsc *gatt_dsc)
{
    struct bletiny_conn *conn;
    struct bletiny_dsc *prev;
    struct bletiny_dsc *dsc;
    struct bletiny_svc *svc;
    struct bletiny_chr *chr;

    conn = bletiny_conn_find(conn_handle);
    if (conn == NULL) {
        BLETINY_LOG(DEBUG, "RECEIVED SERVICE FOR UNKNOWN CONNECTION; "
                           "HANDLE=%d\n",
                    conn_handle);
        return NULL;
    }

    svc = bletiny_svc_find_range(conn, chr_val_handle);
    if (svc == NULL) {
        BLETINY_LOG(DEBUG, "CAN'T FIND SERVICE FOR DISCOVERED DSC; HANDLE=%d\n",
                    conn_handle);
        return NULL;
    }

    chr = bletiny_chr_find(svc, chr_val_handle, NULL);
    if (chr == NULL) {
        BLETINY_LOG(DEBUG, "CAN'T FIND CHARACTERISTIC FOR DISCOVERED DSC; "
                           "HANDLE=%d\n",
                    conn_handle);
        return NULL;
    }

    dsc = bletiny_dsc_find(chr, gatt_dsc->handle, &prev);
    if (dsc != NULL) {
        /* Descriptor already discovered. */
        return dsc;
    }

    dsc = os_memblock_get(&bletiny_dsc_pool);
    if (dsc == NULL) {
        console_printf("OOM WHILE DISCOVERING DESCRIPTOR\n");
        return NULL;
    }
    memset(dsc, 0, sizeof *dsc);

    dsc->dsc = *gatt_dsc;

    if (prev == NULL) {
        SLIST_INSERT_HEAD(&chr->dscs, dsc, next);
    } else {
        SLIST_NEXT(prev, next) = dsc;
    }

    return dsc;
}

static struct bletiny_conn *
bletiny_conn_add(struct ble_gap_conn_desc *desc)
{
    struct bletiny_conn *conn;

    assert(bletiny_num_conns < NIMBLE_OPT(MAX_CONNECTIONS));

    conn = bletiny_conns + bletiny_num_conns;
    bletiny_num_conns++;

    conn->handle = desc->conn_handle;
    SLIST_INIT(&conn->svcs);

    return conn;
}

static void
bletiny_conn_delete_idx(int idx)
{
    struct bletiny_conn *conn;
    struct bletiny_svc *svc;

    assert(idx >= 0 && idx < bletiny_num_conns);

    conn = bletiny_conns + idx;
    while ((svc = SLIST_FIRST(&conn->svcs)) != NULL) {
        SLIST_REMOVE_HEAD(&conn->svcs, next);
        bletiny_svc_delete(svc);
    }

    /* This '#if' is not strictly necessary.  It is here to prevent a spurious
     * warning from being reported.
     */
#if NIMBLE_OPT(MAX_CONNECTIONS) > 1
    int i;
    for (i = idx + 1; i < bletiny_num_conns; i++) {
        bletiny_conns[i - 1] = bletiny_conns[i];
    }
#endif

    bletiny_num_conns--;
}

static int
bletiny_on_mtu(uint16_t conn_handle, const struct ble_gatt_error *error,
               uint16_t mtu, void *arg)
{
    switch (error->status) {
    case 0:
        console_printf("mtu exchange complete: conn_handle=%d mtu=%d\n",
                       conn_handle, mtu);
        break;

    default:
        bletiny_print_error(NULL, conn_handle, error);
        break;
    }

    return 0;
}

static void
bletiny_full_disc_complete(int rc)
{
    console_printf("full discovery complete; rc=%d\n", rc);
    bletiny_full_disc_prev_chr_val = 0;
}

static void
bletiny_disc_full_dscs(uint16_t conn_handle)
{
    struct bletiny_conn *conn;
    struct bletiny_chr *chr;
    struct bletiny_svc *svc;
    int rc;

    conn = bletiny_conn_find(conn_handle);
    if (conn == NULL) {
        BLETINY_LOG(DEBUG, "Failed to discover descriptors for conn=%d; "
                           "not connected\n", conn_handle);
        bletiny_full_disc_complete(BLE_HS_ENOTCONN);
        return;
    }

    SLIST_FOREACH(svc, &conn->svcs, next) {
        SLIST_FOREACH(chr, &svc->chrs, next) {
            if (!chr_is_empty(svc, chr) &&
                SLIST_EMPTY(&chr->dscs) &&
                bletiny_full_disc_prev_chr_val <= chr->chr.def_handle) {

                rc = bletiny_disc_all_dscs(conn_handle,
                                           chr->chr.val_handle,
                                           chr_end_handle(svc, chr));
                if (rc != 0) {
                    bletiny_full_disc_complete(rc);
                }

                bletiny_full_disc_prev_chr_val = chr->chr.val_handle;
                return;
            }
        }
    }

    /* All descriptors discovered. */
    bletiny_full_disc_complete(0);
}

static void
bletiny_disc_full_chrs(uint16_t conn_handle)
{
    struct bletiny_conn *conn;
    struct bletiny_svc *svc;
    int rc;

    conn = bletiny_conn_find(conn_handle);
    if (conn == NULL) {
        BLETINY_LOG(DEBUG, "Failed to discover characteristics for conn=%d; "
                           "not connected\n", conn_handle);
        bletiny_full_disc_complete(BLE_HS_ENOTCONN);
        return;
    }

    SLIST_FOREACH(svc, &conn->svcs, next) {
        if (!svc_is_empty(svc) && SLIST_EMPTY(&svc->chrs)) {
            rc = bletiny_disc_all_chrs(conn_handle, svc->svc.start_handle,
                                       svc->svc.end_handle);
            if (rc != 0) {
                bletiny_full_disc_complete(rc);
            }
            return;
        }
    }

    /* All characteristics discovered. */
    bletiny_disc_full_dscs(conn_handle);
}

static int
bletiny_on_disc_s(uint16_t conn_handle, const struct ble_gatt_error *error,
                  const struct ble_gatt_svc *service, void *arg)
{
    switch (error->status) {
    case 0:
        bletiny_svc_add(conn_handle, service);
        break;

    case BLE_HS_EDONE:
        console_printf("service discovery successful\n");
        if (bletiny_full_disc_prev_chr_val > 0) {
            bletiny_disc_full_chrs(conn_handle);
        }
        break;

    default:
        bletiny_print_error(NULL, conn_handle, error);
        break;
    }

    return 0;
}

static int
bletiny_on_disc_c(uint16_t conn_handle, const struct ble_gatt_error *error,
                  const struct ble_gatt_chr *chr, void *arg)
{
    intptr_t svc_start_handle;

    svc_start_handle = (intptr_t)arg;

    switch (error->status) {
    case 0:
        bletiny_chr_add(conn_handle, svc_start_handle, chr);
        break;

    case BLE_HS_EDONE:
        console_printf("characteristic discovery successful\n");
        if (bletiny_full_disc_prev_chr_val > 0) {
            bletiny_disc_full_chrs(conn_handle);
        }
        break;

    default:
        bletiny_print_error(NULL, conn_handle, error);
        break;
    }

    return 0;
}

static int
bletiny_on_disc_d(uint16_t conn_handle, const struct ble_gatt_error *error,
                  uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc,
                  void *arg)
{
    switch (error->status) {
    case 0:
        bletiny_dsc_add(conn_handle, chr_val_handle, dsc);
        break;

    case BLE_HS_EDONE:
        console_printf("descriptor discovery successful\n");
        if (bletiny_full_disc_prev_chr_val > 0) {
            bletiny_disc_full_dscs(conn_handle);
        }
        break;

    default:
        bletiny_print_error(NULL, conn_handle, error);
        break;
    }

    return 0;
}

static int
bletiny_on_read(uint16_t conn_handle, const struct ble_gatt_error *error,
                struct ble_gatt_attr *attr, void *arg)
{
    switch (error->status) {
    case 0:
        console_printf("characteristic read; conn_handle=%d "
                       "attr_handle=%d len=%d value=", conn_handle,
                       attr->handle, OS_MBUF_PKTLEN(attr->om));
        print_mbuf(attr->om);
        console_printf("\n");
        break;

    case BLE_HS_EDONE:
        console_printf("characteristic read complete\n");
        break;

    default:
        bletiny_print_error(NULL, conn_handle, error);
        break;
    }

    return 0;
}

static int
bletiny_on_write(uint16_t conn_handle, const struct ble_gatt_error *error,
                 struct ble_gatt_attr *attr, void *arg)
{
    switch (error->status) {
    case 0:
        console_printf("characteristic write complete; conn_handle=%d "
                       "attr_handle=%d\n", conn_handle, attr->handle);
        break;

    default:
        bletiny_print_error(NULL, conn_handle, error);
        break;
    }

    return 0;
}

static int
bletiny_on_write_reliable(uint16_t conn_handle,
                          const struct ble_gatt_error *error,
                          struct ble_gatt_attr *attrs, uint8_t num_attrs,
                          void *arg)
{
    int i;

    switch (error->status) {
    case 0:
        console_printf("characteristic write reliable complete; "
                       "conn_handle=%d", conn_handle);

        for (i = 0; i < num_attrs; i++) {
            console_printf(" attr_handle=%d len=%d value=", attrs[i].handle,
                           OS_MBUF_PKTLEN(attrs[i].om));
            print_mbuf(attrs[i].om);
        }
        console_printf("\n");
        break;

    default:
        bletiny_print_error(NULL, conn_handle, error);
        break;
    }

    return 0;
}

static int
bletiny_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int conn_idx;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        console_printf("connection %s; status=%d ",
                       event->connect.status == 0 ? "established" : "failed",
                       event->connect.status);

        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            print_conn_desc(&desc);
            bletiny_conn_add(&desc);
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        console_printf("disconnect; reason=%d ", event->disconnect.reason);
        print_conn_desc(&event->disconnect.conn);

        conn_idx = bletiny_conn_find_idx(event->disconnect.conn.conn_handle);
        if (conn_idx != -1) {
            bletiny_conn_delete_idx(conn_idx);
        }
        return 0;

    case BLE_GAP_EVENT_DISC:
        console_printf("received advertisement; event_type=%d addr_type=%d "
                       "addr=", event->disc.event_type,
                       event->disc.addr_type);
        print_addr(event->disc.addr);
        console_printf(" length_data=%d rssi=%d data=",
                       event->disc.length_data, event->disc.rssi);
        print_bytes(event->disc.data, event->disc.length_data);
        console_printf(" fields:\n");
        bletiny_print_adv_fields(event->disc.fields);
        console_printf("\n");
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        console_printf("scanning finished\n");
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        console_printf("advertising complete.\n");
        return 0;

    case BLE_GAP_EVENT_CONN_CANCEL:
        console_printf("connection procedure cancelled.\n");
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        console_printf("connection updated; status=%d ",
                       event->conn_update.status);
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        assert(rc == 0);
        print_conn_desc(&desc);
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE_REQ:
        console_printf("connection update request\n");
        *event->conn_update_req.self_params =
            *event->conn_update_req.peer_params;
        return 0;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        console_printf("passkey action event; action=%d",
                       event->passkey.params.action);
        if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
            console_printf(" numcmp=%lu",
                           (unsigned long)event->passkey.params.numcmp);
        }
        console_printf("\n");
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        console_printf("encryption change event; status=%d ",
                       event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);
        print_conn_desc(&desc);
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        console_printf("notification rx event; attr_handle=%d indication=%d "
                       "len=%d data=",
                       event->notify_rx.attr_handle,
                       event->notify_rx.indication,
                       OS_MBUF_PKTLEN(event->notify_rx.om));

        print_mbuf(event->notify_rx.om);
        console_printf("\n");
        return 0;

    case BLE_GAP_EVENT_NOTIFY_TX:
        console_printf("notification tx event; status=%d attr_handle=%d "
                       "indication=%d\n",
                       event->notify_tx.status,
                       event->notify_tx.attr_handle,
                       event->notify_tx.indication);
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        console_printf("subscribe event; conn_handle=%d attr_handle=%d "
                       "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                       event->subscribe.conn_handle,
                       event->subscribe.attr_handle,
                       event->subscribe.reason,
                       event->subscribe.prev_notify,
                       event->subscribe.cur_notify,
                       event->subscribe.prev_indicate,
                       event->subscribe.cur_indicate);
        return 0;

    default:
        return 0;
    }
}

static void
bletiny_on_l2cap_update(int status, void *arg)
{
    console_printf("l2cap update complete; status=%d\n", status);
}

static void
bletiny_tx_timer_cb(void *arg)
{
    int i;
    uint8_t len;
    int32_t timeout;
    uint8_t *dptr;
    struct os_mbuf *om;

    if ((bletiny_tx_data.tx_num == 0) || (bletiny_tx_data.tx_len == 0)) {
        return;
    }

    len = bletiny_tx_data.tx_len;
    om = NULL;
    if (default_mbuf_mpool.mp_num_free >= 4) {
        om = os_msys_get_pkthdr(len + 4, sizeof(struct ble_mbuf_hdr));
    }

    if (om) {
        /* Put the HCI header in the mbuf */
        om->om_len = len + 4;
        htole16(om->om_data, bletiny_tx_data.tx_handle);
        htole16(om->om_data + 2, len);
        dptr = om->om_data + 4;

        /*
         * NOTE: first byte gets 0xff so not confused with l2cap channel.
         * The rest of the data gets filled with incrementing pattern starting
         * from 0.
         */
        htole16(dptr, len - 4);
        dptr[2] = 0xff;
        dptr[3] = 0xff;
        dptr += 4;
        len -= 4;

        for (i = 0; i < len; ++i) {
            *dptr = i;
            ++dptr;
        }

        /* Set packet header length */
        OS_MBUF_PKTHDR(om)->omp_len = om->om_len;
        ble_hci_trans_hs_acl_tx(om);

        --bletiny_tx_data.tx_num;
    }

    if (bletiny_tx_data.tx_num) {
        timeout = (int32_t)bletiny_tx_data.tx_rate;
        timeout = (timeout * OS_TICKS_PER_SEC) / 1000;
        os_callout_reset(&bletiny_tx_timer.cf_c, timeout);
    }
}

int
bletiny_exchange_mtu(uint16_t conn_handle)
{
    int rc;

    rc = ble_gattc_exchange_mtu(conn_handle, bletiny_on_mtu, NULL);
    return rc;
}

int
bletiny_disc_all_chrs(uint16_t conn_handle, uint16_t start_handle,
                      uint16_t end_handle)
{
    intptr_t svc_start_handle;
    int rc;

    svc_start_handle = start_handle;
    rc = ble_gattc_disc_all_chrs(conn_handle, start_handle, end_handle,
                                 bletiny_on_disc_c, (void *)svc_start_handle);
    return rc;
}

int
bletiny_disc_chrs_by_uuid(uint16_t conn_handle, uint16_t start_handle,
                           uint16_t end_handle, uint8_t *uuid128)
{
    intptr_t svc_start_handle;
    int rc;

    svc_start_handle = start_handle;
    rc = ble_gattc_disc_chrs_by_uuid(conn_handle, start_handle, end_handle,
                                     uuid128, bletiny_on_disc_c,
                                     &svc_start_handle);
    return rc;
}

int
bletiny_disc_svcs(uint16_t conn_handle)
{
    int rc;

    rc = ble_gattc_disc_all_svcs(conn_handle, bletiny_on_disc_s, NULL);
    return rc;
}

int
bletiny_disc_svc_by_uuid(uint16_t conn_handle, uint8_t *uuid128)
{
    int rc;

    rc = ble_gattc_disc_svc_by_uuid(conn_handle, uuid128,
                                    bletiny_on_disc_s, NULL);
    return rc;
}

int
bletiny_disc_all_dscs(uint16_t conn_handle, uint16_t chr_def_handle,
                      uint16_t chr_end_handle)
{
    int rc;

    rc = ble_gattc_disc_all_dscs(conn_handle, chr_def_handle, chr_end_handle,
                                 bletiny_on_disc_d, NULL);
    return rc;
}

int
bletiny_disc_full(uint16_t conn_handle)
{
    struct bletiny_conn *conn;
    struct bletiny_svc *svc;

    /* Undiscover everything first. */
    conn = bletiny_conn_find(conn_handle);
    if (conn == NULL) {
        return BLE_HS_ENOTCONN;
    }

    while ((svc = SLIST_FIRST(&conn->svcs)) != NULL) {
        SLIST_REMOVE_HEAD(&conn->svcs, next);
        bletiny_svc_delete(svc);
    }

    bletiny_full_disc_prev_chr_val = 1;
    bletiny_disc_svcs(conn_handle);

    return 0;
}

int
bletiny_find_inc_svcs(uint16_t conn_handle, uint16_t start_handle,
                       uint16_t end_handle)
{
    int rc;

    rc = ble_gattc_find_inc_svcs(conn_handle, start_handle, end_handle,
                                 bletiny_on_disc_s, NULL);
    return rc;
}

int
bletiny_read(uint16_t conn_handle, uint16_t attr_handle)
{
    struct os_mbuf *om;
    int rc;

    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        rc = ble_att_svr_read_local(attr_handle, &om);
        if (rc == 0) {
            console_printf("read local; attr_handle=%d len=%d value=",
                           attr_handle, OS_MBUF_PKTLEN(om));
            print_mbuf(om);
            console_printf("\n");

            os_mbuf_free_chain(om);
        }
    } else {
        rc = ble_gattc_read(conn_handle, attr_handle, bletiny_on_read, NULL);
    }
    return rc;
}

int
bletiny_read_long(uint16_t conn_handle, uint16_t attr_handle)
{
    int rc;

    rc = ble_gattc_read_long(conn_handle, attr_handle, bletiny_on_read, NULL);
    return rc;
}

int
bletiny_read_by_uuid(uint16_t conn_handle, uint16_t start_handle,
                      uint16_t end_handle, uint8_t *uuid128)
{
    int rc;

    rc = ble_gattc_read_by_uuid(conn_handle, start_handle, end_handle, uuid128,
                                bletiny_on_read, NULL);
    return rc;
}

int
bletiny_read_mult(uint16_t conn_handle, uint16_t *attr_handles,
                   int num_attr_handles)
{
    int rc;

    rc = ble_gattc_read_mult(conn_handle, attr_handles, num_attr_handles,
                             bletiny_on_read, NULL);
    return rc;
}

int
bletiny_write(uint16_t conn_handle, uint16_t attr_handle, struct os_mbuf *om)
{
    int rc;

    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        rc = ble_att_svr_write_local(attr_handle, om);
    } else {
        rc = ble_gattc_write(conn_handle, attr_handle, om,
                             bletiny_on_write, NULL);
    }

    return rc;
}

int
bletiny_write_no_rsp(uint16_t conn_handle, uint16_t attr_handle,
                     struct os_mbuf *om)
{
    int rc;

    rc = ble_gattc_write_no_rsp(conn_handle, attr_handle, om);

    return rc;
}

int
bletiny_write_long(uint16_t conn_handle, uint16_t attr_handle,
                   struct os_mbuf *om)
{
    int rc;

    rc = ble_gattc_write_long(conn_handle, attr_handle, om,
                              bletiny_on_write, NULL);
    return rc;
}

int
bletiny_write_reliable(uint16_t conn_handle,
                       struct ble_gatt_attr *attrs,
                       int num_attrs)
{
    int rc;

    rc = ble_gattc_write_reliable(conn_handle, attrs, num_attrs,
                                  bletiny_on_write_reliable, NULL);
    return rc;
}

int
bletiny_adv_stop(void)
{
    int rc;

    rc = ble_gap_adv_stop();
    return rc;
}

int
bletiny_adv_start(uint8_t own_addr_type, uint8_t peer_addr_type,
                  const uint8_t *peer_addr, int32_t duration_ms,
                  const struct ble_gap_adv_params *params)
{
    int rc;

    rc = ble_gap_adv_start(own_addr_type, peer_addr_type, peer_addr,
                           duration_ms, params, bletiny_gap_event, NULL);
    return rc;
}

int
bletiny_conn_initiate(uint8_t own_addr_type, uint8_t peer_addr_type,
                      uint8_t *peer_addr, int32_t duration_ms,
                      struct ble_gap_conn_params *params)
{
    int rc;

    rc = ble_gap_connect(own_addr_type, peer_addr_type, peer_addr, duration_ms,
                         params, bletiny_gap_event, NULL);

    return rc;
}

int
bletiny_conn_cancel(void)
{
    int rc;

    rc = ble_gap_conn_cancel();
    return rc;
}

int
bletiny_term_conn(uint16_t conn_handle, uint8_t reason)
{
    int rc;

    rc = ble_gap_terminate(conn_handle, reason);
    return rc;
}

int
bletiny_wl_set(struct ble_gap_white_entry *white_list, int white_list_count)
{
    int rc;

    rc = ble_gap_wl_set(white_list, white_list_count);
    return rc;
}

int
bletiny_scan(uint8_t own_addr_type, int32_t duration_ms,
             const struct ble_gap_disc_params *disc_params)
{
    int rc;

    rc = ble_gap_disc(own_addr_type, duration_ms, disc_params,
                      bletiny_gap_event, NULL);
    return rc;
}

int
bletiny_scan_cancel(void)
{
    int rc;

    rc = ble_gap_disc_cancel();
    return rc;
}

int
bletiny_set_adv_data(struct ble_hs_adv_fields *adv_fields)
{
    int rc;

    rc = ble_gap_adv_set_fields(adv_fields);
    return rc;
}

int
bletiny_update_conn(uint16_t conn_handle, struct ble_gap_upd_params *params)
{
    int rc;

    rc = ble_gap_update_params(conn_handle, params);
    return rc;
}

void
bletiny_chrup(uint16_t attr_handle)
{
    ble_gatts_chr_updated(attr_handle);
}

int
bletiny_datalen(uint16_t conn_handle, uint16_t tx_octets, uint16_t tx_time)
{
    int rc;

    rc = ble_hs_hci_util_set_data_len(conn_handle, tx_octets, tx_time);
    return rc;
}

int
bletiny_l2cap_update(uint16_t conn_handle,
                     struct ble_l2cap_sig_update_params *params)
{
    int rc;

    rc = ble_l2cap_sig_update(conn_handle, params, bletiny_on_l2cap_update,
                              NULL);
    return rc;
}

int
bletiny_sec_pair(uint16_t conn_handle)
{
#if !NIMBLE_OPT(SM)
    return BLE_HS_ENOTSUP;
#endif

    int rc;

    rc = ble_gap_pair_initiate(conn_handle);
    return rc;
}

int
bletiny_sec_start(uint16_t conn_handle)
{
#if !NIMBLE_OPT(SM)
    return BLE_HS_ENOTSUP;
#endif

    int rc;

    rc = ble_gap_security_initiate(conn_handle);
    return rc;
}

int
bletiny_sec_restart(uint16_t conn_handle,
                    uint8_t *ltk,
                    uint16_t ediv,
                    uint64_t rand_val,
                    int auth)
{
#if !NIMBLE_OPT(SM)
    return BLE_HS_ENOTSUP;
#endif

    struct ble_store_value_sec value_sec;
    struct ble_store_key_sec key_sec;
    struct ble_gap_conn_desc desc;
    ble_hs_conn_flags_t conn_flags;
    int rc;

    if (ltk == NULL) {
        /* The user is requesting a store lookup. */
        rc = ble_gap_conn_find(conn_handle, &desc);
        if (rc != 0) {
            return rc;
        }

        memset(&key_sec, 0, sizeof key_sec);
        key_sec.peer_addr_type = desc.peer_id_addr_type;
        memcpy(key_sec.peer_addr, desc.peer_id_addr, 6);

        rc = ble_hs_atomic_conn_flags(conn_handle, &conn_flags);
        if (rc != 0) {
            return rc;
        }
        if (conn_flags & BLE_HS_CONN_F_MASTER) {
            rc = ble_store_read_peer_sec(&key_sec, &value_sec);
        } else {
            rc = ble_store_read_our_sec(&key_sec, &value_sec);
        }
        if (rc != 0) {
            return rc;
        }

        ltk = value_sec.ltk;
        ediv = value_sec.ediv;
        rand_val = value_sec.rand_num;
        auth = value_sec.authenticated;
    }

    rc = ble_gap_encryption_initiate(conn_handle, ltk, ediv, rand_val, auth);
    return rc;
}

/**
 * Called to start transmitting 'num' packets at rate 'rate' of size 'size'
 * to connection handle 'handle'
 *
 * @param handle
 * @param len
 * @param rate
 * @param num
 *
 * @return int
 */
int
bletiny_tx_start(uint16_t handle, uint16_t len, uint16_t rate, uint16_t num)
{
    /* Cannot be currently in a session */
    if (num == 0) {
        return 0;
    }

    /* Do not allow start if already in progress */
    if (bletiny_tx_data.tx_num != 0) {
        return -1;
    }

    /* XXX: for now, must have contiguous mbuf space */
    if ((len + 4) > MBUF_BUF_SIZE) {
        return -2;
    }

    bletiny_tx_data.tx_num = num;
    bletiny_tx_data.tx_rate = rate;
    bletiny_tx_data.tx_len = len;
    bletiny_tx_data.tx_handle = handle;

    os_callout_reset(&bletiny_tx_timer.cf_c, 0);

    return 0;
}

int
bletiny_rssi(uint16_t conn_handle, int8_t *out_rssi)
{
    int rc;

    rc = ble_gap_conn_rssi(conn_handle, out_rssi);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static void
bletiny_on_reset(int reason)
{
    console_printf("Error: Resetting state; reason=%d\n", reason);
}

/**
 * BLE test task
 *
 * @param arg
 */
static void
bletiny_task_handler(void *arg)
{
    struct os_callout_func *cf;
    struct os_event *ev;
    int rc;

    rc = ble_hs_start();
    assert(rc == 0);

    while (1) {
        ev = os_eventq_get(&bletiny_evq);
        switch (ev->ev_type) {
        case OS_EVENT_T_TIMER:
            cf = (struct os_callout_func *)ev;
            assert(cf->cf_func);
            cf->cf_func(CF_ARG(cf));
            break;
        default:
            assert(0);
            break;
        }
    }
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
    struct ble_hci_ram_cfg hci_cfg;
    struct ble_hs_cfg cfg;
    uint32_t seed;
    int rc;
    int i;

    /* Initialize OS */
    os_init();

    /* Set cputime to count at 1 usec increments */
    rc = cputime_init(1000000);
    assert(rc == 0);

    /* Dummy device address */
    memcpy(g_dev_addr, bletiny_addr, 6);

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

    /* Allocate some application specific memory pools. */
    bletiny_svc_mem = malloc(
        OS_MEMPOOL_BYTES(BLETINY_MAX_SVCS, sizeof (struct bletiny_svc)));
    assert(bletiny_svc_mem != NULL);

    rc = os_mempool_init(&bletiny_svc_pool, BLETINY_MAX_SVCS,
                         sizeof (struct bletiny_svc), bletiny_svc_mem,
                         "bletiny_svc_pool");
    assert(rc == 0);

    bletiny_chr_mem = malloc(
        OS_MEMPOOL_BYTES(BLETINY_MAX_CHRS, sizeof (struct bletiny_chr)));
    assert(bletiny_chr_mem != NULL);

    rc = os_mempool_init(&bletiny_chr_pool, BLETINY_MAX_CHRS,
                         sizeof (struct bletiny_chr), bletiny_chr_mem,
                         "bletiny_chr_pool");
    assert(rc == 0);

    bletiny_dsc_mem = malloc(
        OS_MEMPOOL_BYTES(BLETINY_MAX_DSCS, sizeof (struct bletiny_dsc)));
    assert(bletiny_dsc_mem != NULL);

    rc = os_mempool_init(&bletiny_dsc_pool, BLETINY_MAX_DSCS,
                         sizeof (struct bletiny_dsc), bletiny_dsc_mem,
                         "bletiny_dsc_pool");
    assert(rc == 0);

    /* Initialize msys mbufs. */
    rc = os_mempool_init(&default_mbuf_mpool, MBUF_NUM_MBUFS,
                         MBUF_MEMBLOCK_SIZE, default_mbuf_mpool_data,
                         "default_mbuf_data");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&default_mbuf_pool, &default_mbuf_mpool,
                           MBUF_MEMBLOCK_SIZE, MBUF_NUM_MBUFS);
    assert(rc == 0);

    rc = os_msys_register(&default_mbuf_pool);
    assert(rc == 0);

    /* Create the shell task. */
    rc = shell_task_init(SHELL_TASK_PRIO, shell_stack, SHELL_TASK_STACK_SIZE,
                         SHELL_MAX_INPUT_LEN);
    assert(rc == 0);

    /* Initialize the logging system. */
    log_init();
    log_console_handler_init(&bletiny_log_console_handler);
    log_register("bletiny", &bletiny_log, &bletiny_log_console_handler);

    /* Initialize eventq for the application task. */
    os_eventq_init(&bletiny_evq);

    /* Create the bletiny task.  All application logic and NimBLE host
     * operations are performed in this task.
     */
    os_task_init(&bletiny_task, "bletiny", bletiny_task_handler,
                 NULL, BLETINY_TASK_PRIO, OS_WAIT_FOREVER,
                 bletiny_stack, BLETINY_STACK_SIZE);

    rc = stats_module_init();
    assert(rc == 0);

    /* Initialize the BLE LL */
    rc = ble_ll_init(BLE_LL_TASK_PRI, MBUF_NUM_MBUFS, BLE_MBUF_PAYLOAD_SIZE);
    assert(rc == 0);

    /* Initialize the RAM HCI transport. */
    hci_cfg = ble_hci_ram_cfg_dflt;
    rc = ble_hci_ram_init(&hci_cfg);
    assert(rc == 0);

    /* Initialize the NimBLE host configuration. */
    cfg = ble_hs_cfg_dflt;
    cfg.max_hci_bufs = hci_cfg.num_evt_hi_bufs + hci_cfg.num_evt_lo_bufs;
    cfg.max_gattc_procs = 2;
    cfg.reset_cb = bletiny_on_reset;
    cfg.store_read_cb = ble_store_ram_read;
    cfg.store_write_cb = ble_store_ram_write;
    cfg.gatts_register_cb = gatt_svr_register_cb;

    /* Initialize GATT services. */
    rc = ble_svc_gap_init(&cfg);
    assert(rc == 0);

    rc = ble_svc_gatt_init(&cfg);
    assert(rc == 0);

    rc = gatt_svr_init(&cfg);
    assert(rc == 0);

    /* Initialize NimBLE host. */
    rc = ble_hs_init(&bletiny_evq, &cfg);
    assert(rc == 0);

    rc = cmd_init();
    assert(rc == 0);

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("nimble-bletiny");
    assert(rc == 0);

    /* Create a callout (timer).  This callout is used by the "tx" bletiny
     * command to repeatedly send packets of sequential data bytes.
     */
    os_callout_func_init(&bletiny_tx_timer, &bletiny_evq, bletiny_tx_timer_cb,
                         NULL);

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return 0;
}
