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
#include "bletiny_priv.h"

/* BLE */
#include "nimble/ble.h"
#include "nimble/nimble_opt.h"
#include "host/host_hci.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_uuid.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "controller/ble_ll.h"

/* XXX: An app should not include private headers from a library.  The bletiny
 * app uses some of nimble's internal details for logging.
 */
#include "../src/ble_hs_conn.h"
#include "../src/ble_hci_sched.h"

#define BSWAP16(x)  ((uint16_t)(((x) << 8) | (((x) & 0xff00) >> 8)))

/* Nimble task priorities */
#define BLE_LL_TASK_PRI         (OS_TASK_PRI_HIGHEST)
#define HOST_TASK_PRIO          (1)

#define SHELL_TASK_PRIO         (3)
#define SHELL_MAX_INPUT_LEN     (64)
#define SHELL_TASK_STACK_SIZE   (OS_STACK_ALIGN(210))
static bssnz_t os_stack_t shell_stack[SHELL_TASK_STACK_SIZE];

static struct os_mutex bletiny_mutex;

/* Our global device address (public) */
uint8_t g_dev_addr[BLE_DEV_ADDR_LEN];

/* Our random address (in case we need it) */
uint8_t g_random_addr[BLE_DEV_ADDR_LEN];

/* A buffer for host advertising data */
uint8_t g_host_adv_len;

/** Our public address.  Note: this is in reverse byte order. */
static uint8_t bletiny_addr[6] = {0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a};

/* Create a mbuf pool of BLE mbufs */
#define MBUF_NUM_MBUFS      (7)
#define MBUF_BUF_SIZE       OS_ALIGN(BLE_MBUF_PAYLOAD_SIZE, 4)
#define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + BLE_MBUF_MEMBLOCK_OVERHEAD)
#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

os_membuf_t default_mbuf_mpool_data[MBUF_MEMPOOL_SIZE];

struct os_mbuf_pool default_mbuf_pool;
struct os_mempool default_mbuf_mpool;

/* BLETINY variables */
#define BLETINY_STACK_SIZE             (OS_STACK_ALIGN(210))
#define BLETINY_TASK_PRIO              (HOST_TASK_PRIO + 1)

#if NIMBLE_OPT_ROLE_CENTRAL
#define BLETINY_MAX_SVCS               32
#define BLETINY_MAX_CHRS               64
#define BLETINY_MAX_DSCS               64
#else
#define BLETINY_MAX_SVCS               1
#define BLETINY_MAX_CHRS               1
#define BLETINY_MAX_DSCS               1
#endif

struct os_eventq g_bletiny_evq;
struct os_task bletiny_task;
bssnz_t os_stack_t bletiny_stack[BLETINY_STACK_SIZE];

static struct log_handler bletiny_log_console_handler;
struct log bletiny_log;

struct bletiny_conn ble_shell_conns[NIMBLE_OPT_MAX_CONNECTIONS];
int bletiny_num_conns;

void
bletest_inc_adv_pkt_num(void) { }

bssnz_t struct bletiny_conn bletiny_conns[NIMBLE_OPT_MAX_CONNECTIONS];
int bletiny_num_conns;

static void *bletiny_svc_mem;
static struct os_mempool bletiny_svc_pool;

static void *bletiny_chr_mem;
static struct os_mempool bletiny_chr_pool;

static void *bletiny_dsc_mem;
static struct os_mempool bletiny_dsc_pool;

const char *bletiny_device_name = "nimble-bletiny";
const uint16_t bletiny_appearance = BSWAP16(BLE_GAP_APPEARANCE_GEN_COMPUTER);
const uint8_t bletiny_privacy_flag = 0;
uint8_t bletiny_reconnect_addr[6];
uint8_t bletiny_pref_conn_params[8];
uint8_t bletiny_gatt_service_changed[4];

#define XSTR(s) STR(s)
#define STR(s) #s

#ifdef DEVICE_NAME
#define BLETINY_AUTO_DEVICE_NAME    XSTR(DEVICE_NAME)
#else
#define BLETINY_AUTO_DEVICE_NAME    NULL
#endif

void
bletiny_lock(void)
{
    int rc;

    rc = os_mutex_pend(&bletiny_mutex, 0xffffffff);
    assert(rc == 0 || rc == OS_NOT_STARTED);
}

void
bletiny_unlock(void)
{
    int rc;

    rc = os_mutex_release(&bletiny_mutex);
    assert(rc == 0 || rc == OS_NOT_STARTED);
}

static void
bletiny_print_error(char *msg, uint16_t conn_handle,
                    struct ble_gatt_error *error)
{
    console_printf("%s: conn_handle=%d status=%d att_handle=%d\n",
                   msg, conn_handle, error->status, error->att_handle);
}

static void
bletiny_print_bytes(uint8_t *bytes, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        console_printf("%s0x%02x", i != 0 ? ":" : "", bytes[i]);
    }
}

static void
bletiny_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    console_printf("handle=%d peer_addr_type=%d peer_addr=",
                   desc->conn_handle, desc->peer_addr_type);
    bletiny_print_bytes(desc->peer_addr, 6);
    console_printf(" conn_itvl=%d conn_latency=%d supervision_timeout=%d",
                   desc->conn_itvl, desc->conn_latency,
                   desc->supervision_timeout);
}

static void
bletiny_print_sec_params(struct ble_gap_sec_params *sec_params)
{
    console_printf("pair_alg=%d enc_enabled=%d\n", sec_params->pair_alg,
                   sec_params->enc_enabled);
}

static void
bletiny_print_adv_fields(struct ble_hs_adv_fields *fields)
{
    uint32_t u32;
    uint16_t u16;
    uint8_t *u8p;
    int i;

    if (fields->flags_is_present) {
        console_printf("    flags=0x%02x\n", fields->flags);
    }

    if (fields->uuids16 != NULL) {
        console_printf("    uuids16(%scomplete)=",
                       fields->uuids16_is_complete ? "" : "in");
        u8p = fields->uuids16;
        for (i = 0; i < fields->num_uuids16; i++) {
            memcpy(&u16, u8p + i * 2, 2);
            console_printf("0x%04x ", u16);
        }
        console_printf("\n");
    }

    if (fields->uuids32 != NULL) {
        console_printf("    uuids32(%scomplete)=",
                       fields->uuids32_is_complete ? "" : "in");
        u8p = fields->uuids32;
        for (i = 0; i < fields->num_uuids32; i++) {
            memcpy(&u32, u8p + i * 4, 4);
            console_printf("0x%08x ", (unsigned)u32);
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
        console_printf("%*s\n", fields->name_len, fields->name);
    }

    if (fields->tx_pwr_lvl_is_present) {
        console_printf("    tx_pwr_lvl=%d\n", fields->tx_pwr_lvl);
    }

    if (fields->device_class != NULL) {
        console_printf("    device_class=");
        bletiny_print_bytes(fields->device_class,
                            BLE_HS_ADV_DEVICE_CLASS_LEN);
        console_printf("\n");
    }

    if (fields->slave_itvl_range != NULL) {
        console_printf("    slave_itvl_range=");
        bletiny_print_bytes(fields->slave_itvl_range,
                            BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN);
        console_printf("\n");
    }

    if (fields->svc_data_uuid16 != NULL) {
        console_printf("    svc_data_uuid16=");
        bletiny_print_bytes(fields->svc_data_uuid16,
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
        bletiny_print_bytes(fields->le_addr, BLE_HS_ADV_LE_ADDR_LEN);
        console_printf("\n");
    }

    if (fields->le_role_is_present) {
        console_printf("    le_role=0x%02x\n", fields->le_role);
    }

    if (fields->svc_data_uuid32 != NULL) {
        console_printf("    svc_data_uuid32=");
        bletiny_print_bytes(fields->svc_data_uuid32,
                             fields->svc_data_uuid32_len);
        console_printf("\n");
    }

    if (fields->svc_data_uuid128 != NULL) {
        console_printf("    svc_data_uuid128=");
        bletiny_print_bytes(fields->svc_data_uuid128,
                            fields->svc_data_uuid128_len);
        console_printf("\n");
    }

    if (fields->uri != NULL) {
        console_printf("    uri=");
        bletiny_print_bytes(fields->uri, fields->uri_len);
        console_printf("\n");
    }

    if (fields->mfg_data != NULL) {
        console_printf("    mfg_data=");
        bletiny_print_bytes(fields->mfg_data, fields->mfg_data_len);
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

static struct bletiny_conn *
bletiny_conn_add(struct ble_gap_conn_desc *desc)
{
    struct bletiny_conn *conn;

    assert(bletiny_num_conns < NIMBLE_OPT_MAX_CONNECTIONS);

    conn = bletiny_conns + bletiny_num_conns;
    bletiny_num_conns++;

    conn->handle = desc->conn_handle;
    conn->addr_type = desc->peer_addr_type;
    memcpy(conn->addr, desc->peer_addr, 6);
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
        os_memblock_put(&bletiny_svc_pool, svc);
    }

    bletiny_num_conns--;

    /* This '#if' is not strictly necessary.  It is here to prevent a spurious
     * warning from being reported.
     */
#if NIMBLE_OPT_MAX_CONNECTIONS > 1
    int i;
    for (i = idx; i < bletiny_num_conns; i++) {
        bletiny_conns[i - 1] = bletiny_conns[i];
    }
#endif
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

static struct bletiny_svc *
bletiny_svc_add(uint16_t conn_handle, struct ble_gatt_service *gatt_svc)
{
    struct bletiny_conn *conn;
    struct bletiny_svc *prev;
    struct bletiny_svc *svc;

    conn = bletiny_conn_find(conn_handle);
    if (conn == NULL) {
        console_printf("RECEIVED SERVICE FOR UNKNOWN CONNECTION; HANDLE=%d\n",
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
        console_printf("OOM WHILE DISCOVERING SERVICE\n");
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
bletiny_chr_find_prev(struct bletiny_svc *svc, uint16_t chr_def_handle)
{
    struct bletiny_chr *prev;
    struct bletiny_chr *chr;

    prev = NULL;
    SLIST_FOREACH(chr, &svc->chrs, next) {
        if (chr->chr.decl_handle >= chr_def_handle) {
            break;
        }

        prev = chr;
    }

    return prev;
}

static struct bletiny_chr *
bletiny_chr_find(struct bletiny_svc *svc, uint16_t chr_def_handle,
                  struct bletiny_chr **out_prev)
{
    struct bletiny_chr *prev;
    struct bletiny_chr *chr;

    prev = bletiny_chr_find_prev(svc, chr_def_handle);
    if (prev == NULL) {
        chr = SLIST_FIRST(&svc->chrs);
    } else {
        chr = SLIST_NEXT(prev, next);
    }

    if (chr != NULL && chr->chr.decl_handle != chr_def_handle) {
        chr = NULL;
    }

    if (out_prev != NULL) {
        *out_prev = prev;
    }
    return chr;
}


static struct bletiny_chr *
bletiny_chr_add(uint16_t conn_handle,  uint16_t svc_start_handle,
                 struct ble_gatt_chr *gatt_chr)
{
    struct bletiny_conn *conn;
    struct bletiny_chr *prev;
    struct bletiny_chr *chr;
    struct bletiny_svc *svc;

    conn = bletiny_conn_find(conn_handle);
    if (conn == NULL) {
        console_printf("RECEIVED SERVICE FOR UNKNOWN CONNECTION; HANDLE=%d\n",
                       conn_handle);
        return NULL;
    }

    svc = bletiny_svc_find(conn, svc_start_handle, NULL);
    if (svc == NULL) {
        console_printf("CAN'T FIND SERVICE FOR DISCOVERED CHR; HANDLE=%d\n",
                       conn_handle);
        return NULL;
    }

    chr = bletiny_chr_find(svc, gatt_chr->decl_handle, &prev);
    if (chr != NULL) {
        /* Characteristic already discovered. */
        return chr;
    }

    chr = os_memblock_get(&bletiny_chr_pool);
    if (chr == NULL) {
        console_printf("OOM WHILE DISCOVERING CHARACTERISTIC\n");
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
bletiny_dsc_find_prev(struct bletiny_chr *chr, uint16_t dsc_handle)
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
bletiny_dsc_find(struct bletiny_chr *chr, uint16_t dsc_handle,
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
bletiny_dsc_add(uint16_t conn_handle, uint16_t chr_def_handle,
                 struct ble_gatt_dsc *gatt_dsc)
{
    struct bletiny_conn *conn;
    struct bletiny_dsc *prev;
    struct bletiny_dsc *dsc;
    struct bletiny_svc *svc;
    struct bletiny_chr *chr;

    conn = bletiny_conn_find(conn_handle);
    if (conn == NULL) {
        console_printf("RECEIVED SERVICE FOR UNKNOWN CONNECTION; HANDLE=%d\n",
                       conn_handle);
        return NULL;
    }

    svc = bletiny_svc_find_range(conn, chr_def_handle);
    if (svc == NULL) {
        console_printf("CAN'T FIND SERVICE FOR DISCOVERED DSC; HANDLE=%d\n",
                       conn_handle);
        return NULL;
    }

    chr = bletiny_chr_find(svc, chr_def_handle, NULL);
    if (chr == NULL) {
        console_printf("CAN'T FIND CHARACTERISTIC FOR DISCOVERED DSC; "
                       "HANDLE=%d\n", conn_handle);
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

static void
bletiny_start_auto_advertise(void)
{
#if 0
    struct ble_hs_adv_fields fields;
    int rc;

    if (BLETINY_AUTO_DEVICE_NAME != NULL) {
        memset(&fields, 0, sizeof fields);

        fields.name = (uint8_t *)BLETINY_AUTO_DEVICE_NAME;
        fields.name_len = strlen(BLETINY_AUTO_DEVICE_NAME);
        fields.name_is_complete = 1;
        rc = bletiny_set_adv_data(&fields);
        if (rc != 0) {
            return;
        }

        rc = bletiny_adv_start(BLE_GAP_DISC_MODE_GEN, BLE_GAP_CONN_MODE_UND,
                               NULL, 0, NULL);
        if (rc != 0) {
            return;
        }
    }
#endif
}

static int
bletiny_on_mtu(uint16_t conn_handle, struct ble_gatt_error *error,
                uint16_t mtu, void *arg)
{
    if (error != NULL) {
        bletiny_print_error("ERROR EXCHANGING MTU", conn_handle, error);
    } else {
        console_printf("mtu exchange complete: conn_handle=%d mtu=%d\n",
                       conn_handle, mtu);
    }

    return 0;
}

static int
bletiny_on_disc_s(uint16_t conn_handle, struct ble_gatt_error *error,
                   struct ble_gatt_service *service, void *arg)
{
    bletiny_lock();

    if (error != NULL) {
        bletiny_print_error("ERROR DISCOVERING SERVICE", conn_handle, error);
    } else if (service != NULL) {
        bletiny_svc_add(conn_handle, service);
    } else {
        console_printf("service discovery successful\n");
    }

    bletiny_unlock();

    return 0;
}

static int
bletiny_on_disc_c(uint16_t conn_handle, struct ble_gatt_error *error,
                   struct ble_gatt_chr *chr, void *arg)
{
    intptr_t svc_start_handle;

    svc_start_handle = (intptr_t)arg;

    if (error != NULL) {
        bletiny_print_error("ERROR DISCOVERING CHARACTERISTIC", conn_handle,
                             error);
    } else if (chr != NULL) {
        bletiny_lock();
        bletiny_chr_add(conn_handle, svc_start_handle, chr);
        bletiny_unlock();
    } else {
        console_printf("characteristic discovery successful\n");
    }

    return 0;
}

static int
bletiny_on_disc_d(uint16_t conn_handle, struct ble_gatt_error *error,
                   uint16_t chr_def_handle, struct ble_gatt_dsc *dsc,
                   void *arg)
{
    bletiny_lock();

    if (error != NULL) {
        bletiny_print_error("ERROR DISCOVERING DESCRIPTOR", conn_handle,
                             error);
    } else if (dsc != NULL) {
        bletiny_dsc_add(conn_handle, chr_def_handle, dsc);
    } else {
        console_printf("descriptor discovery successful\n");
    }

    bletiny_unlock();

    return 0;
}

static int
bletiny_on_read(uint16_t conn_handle, struct ble_gatt_error *error,
                 struct ble_gatt_attr *attr, void *arg)
{
    if (error != NULL) {
        bletiny_print_error("ERROR READING CHARACTERISTIC", conn_handle,
                             error);
    } else if (attr != NULL) {
        console_printf("characteristic read; conn_handle=%d "
                       "attr_handle=%d len=%d value=", conn_handle,
                       attr->handle, attr->value_len);
        bletiny_print_bytes(attr->value, attr->value_len);
        console_printf("\n");
    } else {
        console_printf("characteristic read complete\n");
    }

    return 0;
}

static int
bletiny_on_read_mult(uint16_t conn_handle, struct ble_gatt_error *error,
                      uint16_t *attr_handles, uint8_t num_attr_handles,
                      uint8_t *attr_data, uint16_t attr_data_len, void *arg)
{
    int i;

    if (error != NULL) {
        bletiny_print_error("ERROR READING CHARACTERISTICS", conn_handle,
                             error);
    } else {
        console_printf("multiple characteristic read complete; conn_handle=%d "
                       "attr_handles=", conn_handle);
        for (i = 0; i < num_attr_handles; i++) {
            console_printf("%s%d", i != 0 ? "," : "", attr_handles[i]);
        }

        console_printf(" len=%d value=", attr_data_len);
        bletiny_print_bytes(attr_data, attr_data_len);
        console_printf("\n");
    }

    return 0;

}

static int
bletiny_on_write(uint16_t conn_handle, struct ble_gatt_error *error,
                  struct ble_gatt_attr *attr, void *arg)
{
    if (error != NULL) {
        bletiny_print_error("ERROR WRITING CHARACTERISTIC", conn_handle,
                             error);
    } else {
        console_printf("characteristic write complete; conn_handle=%d "
                       "attr_handle=%d len=%d value=", conn_handle,
                       attr->handle, attr->value_len);
        bletiny_print_bytes(attr->value, attr->value_len);
        console_printf("\n");
    }

    return 0;
}

static int
bletiny_on_write_reliable(uint16_t conn_handle, struct ble_gatt_error *error,
                           struct ble_gatt_attr *attrs, uint8_t num_attrs,
                           void *arg)
{
    int i;

    if (error != NULL) {
        bletiny_print_error("ERROR WRITING CHARACTERISTICS RELIABLY",
                             conn_handle, error);
    } else {
        console_printf("characteristic write reliable complete; "
                       "conn_handle=%d", conn_handle);

        for (i = 0; i < num_attrs; i++) {
            console_printf(" attr_handle=%d len=%d value=", attrs[i].handle,
                           attrs[i].value_len);
            bletiny_print_bytes(attrs[i].value, attrs[i].value_len);
        }
        console_printf("\n");
    }

    return 0;
}

static int
bletiny_on_notify(uint16_t conn_handle, uint16_t attr_handle,
                   uint8_t *attr_val, uint16_t attr_len, void *arg)
{
    console_printf("received notification from conn_handle=%d attr=%d "
                   "len=%d value=", conn_handle, attr_handle, attr_len);

    bletiny_print_bytes(attr_val, attr_len);
    console_printf("\n");

    return 0;
}

static int
bletiny_on_connect(int event, int status, struct ble_gap_conn_ctxt *ctxt,
                   void *arg)
{
    int conn_idx;

    bletiny_lock();

    switch (event) {
    case BLE_GAP_EVENT_CONN:
        console_printf("connection %s; status=%d ",
                       status == 0 ? "up" : "down", status);
        bletiny_print_conn_desc(ctxt->desc);
        console_printf("\n");

        if (status == 0) {
            bletiny_conn_add(ctxt->desc);
        } else {
            if (ctxt->desc->conn_handle == BLE_HS_CONN_HANDLE_NONE) {
                if (status == BLE_HS_HCI_ERR(BLE_ERR_UNK_CONN_ID)) {
                    console_printf("connection procedure cancelled.\n");
                }
            } else {
                conn_idx = bletiny_conn_find_idx(ctxt->desc->conn_handle);
                if (conn_idx == -1) {
                    console_printf("UNKNOWN CONNECTION\n");
                } else {
                    bletiny_conn_delete_idx(conn_idx);
                }

                bletiny_start_auto_advertise();
            }
        }

        break;

    case BLE_GAP_EVENT_CONN_UPDATED:
        console_printf("connection updated; status=%d ", status);
        bletiny_print_conn_desc(ctxt->desc);
        console_printf("\n");
        break;

    case BLE_GAP_EVENT_CONN_UPDATE_REQ:
        console_printf("connection update request; status=%d ", status);
        *ctxt->update.self_params = *ctxt->update.peer_params;
        break;

    case BLE_GAP_EVENT_SECURITY:
        console_printf("security event; status=%d ", status);
        bletiny_print_sec_params(ctxt->sec_params);
        break;
    }

    bletiny_unlock();

    return 0;
}

static void
bletiny_on_l2cap_update(int status, void *arg)
{
    console_printf("l2cap update complete; status=%d\n", status);
}

static void
bletiny_on_scan(int event, int status, struct ble_gap_disc_desc *desc,
                 void *arg)
{
    switch (event) {
    case BLE_GAP_EVENT_DISC_SUCCESS:
        console_printf("received advertisement; event_type=%d addr_type=%d "
                       "addr=", desc->event_type, desc->addr_type);
        bletiny_print_bytes(desc->addr, 6);
        console_printf(" length_data=%d rssi=%d data=", desc->length_data,
                       desc->rssi);
        bletiny_print_bytes(desc->data, desc->length_data);
        console_printf(" fields:\n");
        bletiny_print_adv_fields(desc->fields);
        console_printf("\n");
        break;

    case BLE_GAP_EVENT_DISC_FINISHED:
        console_printf("scanning finished; status=%d\n", status);
        break;

    default:
        assert(0);
        break;
    }
}

static void
bletiny_on_rx_rssi(struct ble_hci_ack *ack, void *unused)
{
    struct hci_read_rssi_ack_params params;

    if (ack->bha_params_len != BLE_HCI_READ_RSSI_ACK_PARAM_LEN) {
        console_printf("invalid rssi response; len=%d\n",
                       ack->bha_params_len);
        return;
    }

    params.status = ack->bha_params[0];
    params.connection_handle = le16toh(ack->bha_params + 1);
    params.rssi = ack->bha_params[3];

    console_printf("rssi response received; status=%d conn=%d rssi=%d\n",
                   params.status, params.connection_handle, params.rssi);
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
    int rc;

    rc = ble_gattc_read(conn_handle, attr_handle, bletiny_on_read, NULL);
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
                             bletiny_on_read_mult, NULL);
    return rc;
}

int
bletiny_write(uint16_t conn_handle, uint16_t attr_handle, void *value,
               uint16_t value_len)
{
    int rc;

    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        rc = ble_att_svr_write_local(attr_handle, value, value_len);
    } else {
        rc = ble_gattc_write(conn_handle, attr_handle, value, value_len,
                             bletiny_on_write, NULL);
    }

    return rc;
}

int
bletiny_write_no_rsp(uint16_t conn_handle, uint16_t attr_handle, void *value,
                      uint16_t value_len)
{
    int rc;

    rc = ble_gattc_write_no_rsp(conn_handle, attr_handle, value, value_len,
                                bletiny_on_write, NULL);
    return rc;
}

int
bletiny_write_long(uint16_t conn_handle, uint16_t attr_handle, void *value,
                    uint16_t value_len)
{
    int rc;

    rc = ble_gattc_write_long(conn_handle, attr_handle, value, value_len,
                              bletiny_on_write, NULL);
    return rc;
}

int
bletiny_write_reliable(uint16_t conn_handle, struct ble_gatt_attr *attrs,
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
bletiny_adv_start(int disc, int conn, uint8_t *peer_addr, int addr_type,
                  struct hci_adv_params *params)
{
    int rc;

    rc = ble_gap_adv_start(disc, conn, peer_addr, addr_type, params,
                           bletiny_on_connect, NULL);
    return rc;
}

int
bletiny_conn_initiate(int addr_type, uint8_t *peer_addr,
                       struct ble_gap_crt_params *params)
{
    int rc;

    rc = ble_gap_conn_initiate(addr_type, peer_addr, NULL, bletiny_on_connect,
                               params);
    return rc;
}

int
bletiny_conn_cancel(void)
{
    int rc;

    rc = ble_gap_cancel();
    return rc;
}

int
bletiny_term_conn(uint16_t conn_handle)
{
    int rc;

    rc = ble_gap_terminate(conn_handle);
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
bletiny_scan(uint32_t dur_ms, uint8_t disc_mode, uint8_t scan_type,
              uint8_t filter_policy)
{
    int rc;

    rc = ble_gap_disc(dur_ms, disc_mode, scan_type, filter_policy,
                      bletiny_on_scan, NULL);
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
bletiny_l2cap_update(uint16_t conn_handle,
                     struct ble_l2cap_sig_update_params *params)
{
    int rc;

    rc = ble_l2cap_sig_update(conn_handle, params, bletiny_on_l2cap_update,
                              NULL);
    return rc;
}

static int
bletiny_tx_rssi_req(void *arg)
{
    intptr_t conn_handle;
    int rc;

    conn_handle = (intptr_t)arg;

    ble_hci_sched_set_ack_cb(bletiny_on_rx_rssi, NULL);

    rc = host_hci_cmd_read_rssi(conn_handle);
    if (rc != 0) {
        console_printf("failure to send rssi hci cmd; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

int
bletiny_show_rssi(uint16_t conn_handle)
{
    int rc;

    rc = ble_hci_sched_enqueue(bletiny_tx_rssi_req,
                               (void *)(intptr_t)conn_handle, NULL);
    if (rc != 0) {
        console_printf("failure to enqueue rssi hci cmd; rc=%d\n", rc);
        return rc;
    }

    return 0;

}

int
bletiny_sec_start(uint16_t conn_handle)
{
#if !NIMBLE_OPT_SM
    return BLE_HS_ENOTSUP;
#endif

    int rc;

    rc = ble_gap_security_initiate(conn_handle);
    return rc;
}

/**
 * BLE test task
 *
 * @param arg
 */
static void
bletiny_task_handler(void *arg)
{
    struct os_event *ev;
    struct os_callout_func *cf;

    periph_init();

    ble_att_set_notify_cb(bletiny_on_notify, NULL);

    /* Initialize eventq */
    os_eventq_init(&g_bletiny_evq);

    bletiny_start_auto_advertise();

    while (1) {
        ev = os_eventq_get(&g_bletiny_evq);
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

    rc = os_mempool_init(&default_mbuf_mpool, MBUF_NUM_MBUFS, 
                         MBUF_MEMBLOCK_SIZE, default_mbuf_mpool_data, 
                         "default_mbuf_data");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&default_mbuf_pool, &default_mbuf_mpool,
                           MBUF_MEMBLOCK_SIZE, MBUF_NUM_MBUFS);
    assert(rc == 0);

    rc = os_msys_register(&default_mbuf_pool);
    assert(rc == 0);

    log_init();
    log_console_handler_init(&bletiny_log_console_handler);
    log_register("bletiny", &bletiny_log, &bletiny_log_console_handler);

    os_task_init(&bletiny_task, "bletiny", bletiny_task_handler,
                 NULL, BLETINY_TASK_PRIO, OS_WAIT_FOREVER,
                 bletiny_stack, BLETINY_STACK_SIZE);

    rc = shell_task_init(SHELL_TASK_PRIO, shell_stack, SHELL_TASK_STACK_SIZE,
                         SHELL_MAX_INPUT_LEN);
    assert(rc == 0);

    /* Init the console */
    rc = console_init(shell_console_rx_cb);
    assert(rc == 0);

    rc = stats_module_init();
    assert(rc == 0);

    /* Initialize the BLE host. */
    cfg = ble_hs_cfg_dflt;
    cfg.max_hci_bufs = 3;
    cfg.max_attrs = 32;
    cfg.max_services = 4;
    cfg.max_client_configs = 6;
    cfg.max_gattc_procs = 2;
    cfg.max_l2cap_chans = 3;
    cfg.max_l2cap_sig_procs = 2;

    rc = ble_hs_init(HOST_TASK_PRIO, &cfg);
    assert(rc == 0);

    /* Initialize the BLE LL */
    rc = ble_ll_init(BLE_LL_TASK_PRI, MBUF_NUM_MBUFS, BLE_MBUF_PAYLOAD_SIZE);
    assert(rc == 0);

    rc = cmd_init();
    assert(rc == 0);

    /* Initialize the preferred parameters. */
    htole16(bletiny_pref_conn_params + 0, BLE_GAP_INITIAL_CONN_ITVL_MIN);
    htole16(bletiny_pref_conn_params + 2, BLE_GAP_INITIAL_CONN_ITVL_MAX);
    htole16(bletiny_pref_conn_params + 4, 0);
    htole16(bletiny_pref_conn_params + 6, BSWAP16(0x100));

    rc = os_mutex_init(&bletiny_mutex);
    assert(rc == 0);

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return 0;
}
