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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "bsp/bsp.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_cputime.h"
#include "console/console.h"
#include "shell/shell.h"
#include "bleshell_priv.h"

/* BLE */
#include "nimble/ble.h"
#include "host/host_hci.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_uuid.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "controller/ble_ll.h"

#define BSWAP16(x)  ((uint16_t)(((x) << 8) | (((x) & 0xff00) >> 8)))

/* Task 1 */
#define HOST_TASK_PRIO          (1)

#define SHELL_TASK_PRIO         (3) 
#define SHELL_TASK_STACK_SIZE   (OS_STACK_ALIGN(384))
os_stack_t shell_stack[SHELL_TASK_STACK_SIZE];

/* For LED toggling */
int g_led_pin;

/* Our global device address (public) */
uint8_t g_dev_addr[BLE_DEV_ADDR_LEN];

/* Our random address (in case we need it) */
uint8_t g_random_addr[BLE_DEV_ADDR_LEN];

/* A buffer for host advertising data */
uint8_t g_host_adv_data[BLE_HCI_MAX_ADV_DATA_LEN];
uint8_t g_host_adv_len;

static uint8_t bleshell_addr[6] = {0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a};

/* Create a mbuf pool of BLE mbufs */
#define MBUF_NUM_MBUFS      (8)
#define MBUF_BUF_SIZE       (256 + sizeof(struct hci_data_hdr))
#define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + BLE_MBUF_PKT_OVERHEAD)

#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

struct os_mbuf_pool g_mbuf_pool; 
struct os_mempool g_mbuf_mempool;
os_membuf_t g_mbuf_buffer[MBUF_MEMPOOL_SIZE];

/* BLESHELL variables */
#define BLESHELL_STACK_SIZE             (128)
#define BLESHELL_TASK_PRIO              (HOST_TASK_PRIO + 1)

#define BLESHELL_MAX_SVCS               8
#define BLESHELL_MAX_CHRS               32
#define BLESHELL_MAX_DSCS               32

uint32_t g_next_os_time;
int g_bleshell_state;
struct os_eventq g_bleshell_evq;
struct os_task bleshell_task;
bssnz_t os_stack_t bleshell_stack[BLESHELL_STACK_SIZE];

struct bleshell_conn ble_shell_conns[BLESHELL_MAX_CONNS];
int bleshell_num_conns;

void
bletest_inc_adv_pkt_num(void) { }

bssnz_t struct bleshell_conn bleshell_conns[BLESHELL_MAX_CONNS];
int bleshell_num_conns;

static void *bleshell_svc_mem;
static struct os_mempool bleshell_svc_pool;

static void *bleshell_chr_mem;
static struct os_mempool bleshell_chr_pool;

static void *bleshell_dsc_mem;
static struct os_mempool bleshell_dsc_pool;

const char *bleshell_device_name = "mynewt nimble";
const uint16_t bleshell_appearance = BSWAP16(BLE_GAP_APPEARANCE_GEN_COMPUTER);
const uint8_t bleshell_privacy_flag = 0;
uint8_t bleshell_reconnect_addr[6];
uint8_t bleshell_pref_conn_params[8];
uint8_t bleshell_gatt_service_changed[4];

static void
bleshell_print_error(char *msg, uint16_t conn_handle,
                     struct ble_gatt_error *error)
{
    console_printf("%s: conn_handle=%d status=%d att_handle=%d\n",
                   msg, conn_handle, error->status, error->att_handle);
}

static void
bleshell_print_bytes(uint8_t *bytes, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        console_printf("%s0x%02x", i != 0 ? ":" : "", bytes[i]);
    }
}

static void
bleshell_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    console_printf("handle=%d peer_addr_type=%d peer_addr=",
                   desc->conn_handle, desc->peer_addr_type);
    bleshell_print_bytes(desc->peer_addr, 6);
    console_printf(" conn_itvl=%d conn_latency=%d supervision_timeout=%d",
                   desc->conn_itvl, desc->conn_latency,
                   desc->supervision_timeout);
}

static void
bleshell_print_adv_fields(struct ble_hs_adv_fields *fields)
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
        bleshell_print_bytes(fields->device_class,
                             BLE_HS_ADV_DEVICE_CLASS_LEN);
    }

    if (fields->slave_itvl_range != NULL) {
        console_printf("    slave_itvl_range=");
        bleshell_print_bytes(fields->slave_itvl_range,
                             BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN);
    }

    if (fields->svc_data_uuid16 != NULL) {
        console_printf("    svc_data_uuid16=");
        bleshell_print_bytes(fields->svc_data_uuid16,
                             fields->svc_data_uuid16_len);
    }

    if (fields->public_tgt_addr != NULL) {
        console_printf("    public_tgt_addr=");
        u8p = fields->public_tgt_addr;
        for (i = 0; i < fields->num_public_tgt_addrs; i++) {
            print_addr(u8p);
            u8p += BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN;
        }
    }

    if (fields->appearance_is_present) {
        console_printf("    appearance=0x%04x\n", fields->appearance);
    }

    if (fields->adv_itvl_is_present) {
        console_printf("    adv_itvl=0x%04x\n", fields->adv_itvl);
    }

    if (fields->le_addr != NULL) {
        console_printf("    le_addr=");
        bleshell_print_bytes(fields->le_addr, BLE_HS_ADV_LE_ADDR_LEN);
    }

    if (fields->le_role_is_present) {
        console_printf("    le_role=0x%02x\n", fields->le_role);
    }

    if (fields->svc_data_uuid32 != NULL) {
        console_printf("    svc_data_uuid32=");
        bleshell_print_bytes(fields->svc_data_uuid32,
                             fields->svc_data_uuid32_len);
    }

    if (fields->svc_data_uuid128 != NULL) {
        console_printf("    svc_data_uuid128=");
        bleshell_print_bytes(fields->svc_data_uuid128,
                             fields->svc_data_uuid128_len);
    }

    if (fields->uri != NULL) {
        console_printf("    uri=");
        bleshell_print_bytes(fields->uri, fields->uri_len);
    }

    if (fields->mfg_data != NULL) {
        console_printf("    mfg_data=");
        bleshell_print_bytes(fields->mfg_data, fields->mfg_data_len);
    }
}

static int
bleshell_conn_find_idx(uint16_t handle)
{
    int i;

    for (i = 0; i < bleshell_num_conns; i++) {
        if (bleshell_conns[i].handle == handle) {
            return i;
        }
    }

    return -1;
}

static struct bleshell_conn *
bleshell_conn_find(uint16_t handle)
{
    int idx;

    idx = bleshell_conn_find_idx(handle);
    if (idx == -1) {
        return NULL;
    } else {
        return bleshell_conns + idx;
    }
}

static struct bleshell_conn *
bleshell_conn_add(struct ble_gap_conn_desc *desc)
{
    struct bleshell_conn *conn;

    assert(bleshell_num_conns < BLESHELL_MAX_CONNS);

    conn = bleshell_conns + bleshell_num_conns;
    bleshell_num_conns++;

    conn->handle = desc->conn_handle;
    conn->addr_type = desc->peer_addr_type;
    memcpy(conn->addr, desc->peer_addr, 6);
    SLIST_INIT(&conn->svcs);

    return conn;
}

static void
bleshell_conn_delete_idx(int idx)
{
    struct bleshell_conn *conn;
    struct bleshell_svc *svc;
    int i;

    assert(idx >= 0 && idx < bleshell_num_conns);

    conn = bleshell_conns + idx;
    while ((svc = SLIST_FIRST(&conn->svcs)) != NULL) {
        SLIST_REMOVE_HEAD(&conn->svcs, next);
        os_memblock_put(&bleshell_svc_pool, svc);
    }

    bleshell_num_conns--;
    for (i = idx; i < bleshell_num_conns; i++) {
        bleshell_conns[i - 1] = bleshell_conns[i];
    }
}

static struct bleshell_svc *
bleshell_svc_find_prev(struct bleshell_conn *conn, uint16_t svc_start_handle)
{
    struct bleshell_svc *prev;
    struct bleshell_svc *svc;

    prev = NULL;
    SLIST_FOREACH(svc, &conn->svcs, next) {
        if (svc->svc.start_handle >= svc_start_handle) {
            break;
        }

        prev = svc;
    }

    return prev;
}

static struct bleshell_svc *
bleshell_svc_find(struct bleshell_conn *conn, uint16_t svc_start_handle,
                  struct bleshell_svc **out_prev)
{
    struct bleshell_svc *prev;
    struct bleshell_svc *svc;

    prev = bleshell_svc_find_prev(conn, svc_start_handle);
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

static struct bleshell_svc *
bleshell_svc_find_range(struct bleshell_conn *conn, uint16_t attr_handle)
{
    struct bleshell_svc *svc;

    SLIST_FOREACH(svc, &conn->svcs, next) {
        if (svc->svc.start_handle <= attr_handle &&
            svc->svc.end_handle >= attr_handle) {

            return svc;
        }
    }

    return NULL;
}

static struct bleshell_svc *
bleshell_svc_add(uint16_t conn_handle, struct ble_gatt_service *gatt_svc)
{
    struct bleshell_conn *conn;
    struct bleshell_svc *prev;
    struct bleshell_svc *svc;

    conn = bleshell_conn_find(conn_handle);
    if (conn == NULL) {
        console_printf("RECEIVED SERVICE FOR UNKNOWN CONNECTION; HANDLE=%d\n",
                       conn_handle);
        return NULL;
    }

    svc = bleshell_svc_find(conn, gatt_svc->start_handle, &prev);
    if (svc != NULL) {
        /* Service already discovered. */
        return svc;
    }

    svc = os_memblock_get(&bleshell_svc_pool);
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

static struct bleshell_chr *
bleshell_chr_find_prev(struct bleshell_svc *svc, uint16_t chr_def_handle)
{
    struct bleshell_chr *prev;
    struct bleshell_chr *chr;

    prev = NULL;
    SLIST_FOREACH(chr, &svc->chrs, next) {
        if (chr->chr.decl_handle >= chr_def_handle) {
            break;
        }

        prev = chr;
    }

    return prev;
}

static struct bleshell_chr *
bleshell_chr_find(struct bleshell_svc *svc, uint16_t chr_def_handle,
                  struct bleshell_chr **out_prev)
{
    struct bleshell_chr *prev;
    struct bleshell_chr *chr;

    prev = bleshell_chr_find_prev(svc, chr_def_handle);
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


static struct bleshell_chr *
bleshell_chr_add(uint16_t conn_handle,  uint16_t svc_start_handle,
                 struct ble_gatt_chr *gatt_chr)
{
    struct bleshell_conn *conn;
    struct bleshell_chr *prev;
    struct bleshell_chr *chr;
    struct bleshell_svc *svc;

    conn = bleshell_conn_find(conn_handle);
    if (conn == NULL) {
        console_printf("RECEIVED SERVICE FOR UNKNOWN CONNECTION; HANDLE=%d\n",
                       conn_handle);
        return NULL;
    }

    svc = bleshell_svc_find(conn, svc_start_handle, NULL);
    if (svc == NULL) {
        console_printf("CAN'T FIND SERVICE FOR DISCOVERED CHR; HANDLE=%d\n",
                       conn_handle);
        return NULL;
    }

    chr = bleshell_chr_find(svc, gatt_chr->decl_handle, &prev);
    if (chr != NULL) {
        /* Characteristic already discovered. */
        return chr;
    }

    chr = os_memblock_get(&bleshell_chr_pool);
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

static struct bleshell_dsc *
bleshell_dsc_find_prev(struct bleshell_chr *chr, uint16_t dsc_handle)
{
    struct bleshell_dsc *prev;
    struct bleshell_dsc *dsc;

    prev = NULL;
    SLIST_FOREACH(dsc, &chr->dscs, next) {
        if (dsc->dsc.handle >= dsc_handle) {
            break;
        }

        prev = dsc;
    }

    return prev;
}

static struct bleshell_dsc *
bleshell_dsc_find(struct bleshell_chr *chr, uint16_t dsc_handle,
                  struct bleshell_dsc **out_prev)
{
    struct bleshell_dsc *prev;
    struct bleshell_dsc *dsc;

    prev = bleshell_dsc_find_prev(chr, dsc_handle);
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

static struct bleshell_dsc *
bleshell_dsc_add(uint16_t conn_handle, uint16_t chr_def_handle,
                 struct ble_gatt_dsc *gatt_dsc)
{
    struct bleshell_conn *conn;
    struct bleshell_dsc *prev;
    struct bleshell_dsc *dsc;
    struct bleshell_svc *svc;
    struct bleshell_chr *chr;

    conn = bleshell_conn_find(conn_handle);
    if (conn == NULL) {
        console_printf("RECEIVED SERVICE FOR UNKNOWN CONNECTION; HANDLE=%d\n",
                       conn_handle);
        return NULL;
    }

    svc = bleshell_svc_find_range(conn, chr_def_handle);
    if (svc == NULL) {
        console_printf("CAN'T FIND SERVICE FOR DISCOVERED DSC; HANDLE=%d\n",
                       conn_handle);
        return NULL;
    }

    chr = bleshell_chr_find(svc, chr_def_handle, NULL);
    if (chr == NULL) {
        console_printf("CAN'T FIND CHARACTERISTIC FOR DISCOVERED DSC; "
                       "HANDLE=%d\n", conn_handle);
        return NULL;
    }

    dsc = bleshell_dsc_find(chr, gatt_dsc->handle, &prev);
    if (dsc != NULL) {
        /* Descriptor already discovered. */
        return dsc;
    }

    dsc = os_memblock_get(&bleshell_dsc_pool);
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

static int
bleshell_on_mtu(uint16_t conn_handle, struct ble_gatt_error *error,
                uint16_t mtu, void *arg)
{
    if (error != NULL) {
        bleshell_print_error("ERROR EXCHANGING MTU", conn_handle, error);
    } else {
        console_printf("mtu exchange complete: conn_handle=%d mtu=%d\n",
                       conn_handle, mtu);
    }

    return 0;
}

static int
bleshell_on_disc_s(uint16_t conn_handle, struct ble_gatt_error *error,
                   struct ble_gatt_service *service, void *arg)
{
    if (error != NULL) {
        bleshell_print_error("ERROR DISCOVERING SERVICE", conn_handle, error);
    } else if (service != NULL) {
        bleshell_svc_add(conn_handle, service);
    } else {
        /* Service discovery complete. */
    }

    return 0;
}

static int
bleshell_on_disc_c(uint16_t conn_handle, struct ble_gatt_error *error,
                   struct ble_gatt_chr *chr, void *arg)
{
    intptr_t *svc_start_handle;

    svc_start_handle = arg;

    if (error != NULL) {
        bleshell_print_error("ERROR DISCOVERING CHARACTERISTIC", conn_handle,
                             error);
    } else if (chr != NULL) {
        bleshell_chr_add(conn_handle, *svc_start_handle, chr);
    } else {
        /* Service discovery complete. */
    }

    return 0;
}

static int
bleshell_on_disc_d(uint16_t conn_handle, struct ble_gatt_error *error,
                   uint16_t chr_val_handle, struct ble_gatt_dsc *dsc,
                   void *arg)
{
    intptr_t *chr_def_handle;

    chr_def_handle = arg;

    if (error != NULL) {
        bleshell_print_error("ERROR DISCOVERING DESCRIPTOR", conn_handle,
                             error);
    } else if (dsc != NULL) {
        bleshell_dsc_add(conn_handle, *chr_def_handle, dsc);
    } else {
        /* Descriptor discovery complete. */
    }

    return 0;
}

static int
bleshell_on_read(uint16_t conn_handle, struct ble_gatt_error *error,
                 struct ble_gatt_attr *attr, void *arg)
{
    if (error != NULL) {
        bleshell_print_error("ERROR READING CHARACTERISTIC", conn_handle,
                             error);
    } else if (attr != NULL) {
        console_printf("characteristic read; conn_handle=%d "
                       "attr_handle=%d len=%d value=", conn_handle,
                       attr->handle, attr->value_len);
        bleshell_print_bytes(attr->value, attr->value_len);
        console_printf("\n");
    } else {
        console_printf("characteristic read complete\n");
    }

    return 0;
}

static int
bleshell_on_read_mult(uint16_t conn_handle, struct ble_gatt_error *error,
                      uint16_t *attr_handles, uint8_t num_attr_handles,
                      uint8_t *attr_data, uint16_t attr_data_len, void *arg)
{
    int i;

    if (error != NULL) {
        bleshell_print_error("ERROR READING CHARACTERISTICS", conn_handle,
                             error);
    } else {
        console_printf("multiple characteristic read complete; conn_handle=%d "
                       "attr_handles=", conn_handle);
        for (i = 0; i < num_attr_handles; i++) {
            console_printf("%s%d", i != 0 ? "," : "", attr_handles[i]);
        }

        console_printf(" len=%d value=", attr_data_len);
        bleshell_print_bytes(attr_data, attr_data_len);
        console_printf("\n");
    }

    return 0;

}

static int
bleshell_on_write(uint16_t conn_handle, struct ble_gatt_error *error,
                  struct ble_gatt_attr *attr, void *arg)
{
    if (error != NULL) {
        bleshell_print_error("ERROR WRITING CHARACTERISTIC", conn_handle,
                             error);
    } else {
        console_printf("characteristic write complete; conn_handle=%d "
                       "attr_handle=%d len=%d value=", conn_handle,
                       attr->handle, attr->value_len);
        bleshell_print_bytes(attr->value, attr->value_len);
        console_printf("\n");
    }

    return 0;
}

static int
bleshell_on_write_reliable(uint16_t conn_handle, struct ble_gatt_error *error,
                           struct ble_gatt_attr *attrs, uint8_t num_attrs,
                           void *arg)
{
    int i;

    if (error != NULL) {
        bleshell_print_error("ERROR WRITING CHARACTERISTICS RELIABLY",
                             conn_handle, error);
    } else {
        console_printf("characteristic write reliable complete; "
                       "conn_handle=%d", conn_handle);

        for (i = 0; i < num_attrs; i++) {
            console_printf(" attr_handle=%d len=%d value=", attrs[i].handle,
                           attrs[i].value_len);
            bleshell_print_bytes(attrs[i].value, attrs[i].value_len);
        }
        console_printf("\n");
    }

    return 0;
}

static int
bleshell_on_notify(uint16_t conn_handle, uint16_t attr_handle,
                   uint8_t *attr_val, uint16_t attr_len, void *arg)
{
    console_printf("received notification from conn_handle=%d attr=%d "
                   "len=%d value=", conn_handle, attr_handle, attr_len);

    bleshell_print_bytes(attr_val, attr_len);
    console_printf("\n");

    return 0;
}

static int
bleshell_on_connect(int event, int status, struct ble_gap_conn_ctxt *ctxt,
                    void *arg)
{
    int conn_idx;

    switch (event) {
    case BLE_GAP_EVENT_CONN:
        console_printf("connection complete; status=%d ", status);
        bleshell_print_conn_desc(&ctxt->desc);
        console_printf("\n");

        if (status == 0) {
            bleshell_conn_add(&ctxt->desc);
        } else {
            if (ctxt->desc.conn_handle == BLE_HS_CONN_HANDLE_NONE) {
                if (status == BLE_HS_HCI_ERR(BLE_ERR_UNK_CONN_ID)) {
                    console_printf("connection procedure cancelled.\n");
                }
            } else {
                conn_idx = bleshell_conn_find_idx(ctxt->desc.conn_handle);
                if (conn_idx == -1) {
                    console_printf("UNKNOWN CONNECTION\n");
                } else {
                    bleshell_conn_delete_idx(conn_idx);
                }
            }
        }

        break;

    case BLE_GAP_EVENT_CONN_UPDATED:
        console_printf("connection updated; status=%d ", status);
        bleshell_print_conn_desc(&ctxt->desc);
        console_printf("\n");
        break;
    }

    return 0;
}

static void
bleshell_on_wl_set(int status, void *arg)
{
    console_printf("white list set status=%d\n", status);
}

static void
bleshell_on_scan(int event, int status, struct ble_gap_disc_desc *desc,
                 void *arg)
{
    switch (event) {
    case BLE_GAP_EVENT_DISC_SUCCESS:
        console_printf("received advertisement; event_type=%d addr_type=%d "
                       "addr=", desc->event_type, desc->addr_type);
        bleshell_print_bytes(desc->addr, 6);
        console_printf(" length_data=%d rssi=%d data=", desc->length_data,
                       desc->rssi);
        bleshell_print_bytes(desc->data, desc->length_data);
        console_printf(" fields:\n");
        bleshell_print_adv_fields(desc->fields);
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

int
bleshell_exchange_mtu(uint16_t conn_handle)
{
    int rc;

    rc = ble_gattc_exchange_mtu(conn_handle, bleshell_on_mtu, NULL);
    return rc;
}

int
bleshell_disc_all_chrs(uint16_t conn_handle, uint16_t start_handle,
                       uint16_t end_handle)
{
    intptr_t svc_start_handle;
    int rc;

    svc_start_handle = start_handle;
    rc = ble_gattc_disc_all_chrs(conn_handle, start_handle, end_handle,
                                 bleshell_on_disc_c, &svc_start_handle);
    return rc;
}

int
bleshell_disc_chrs_by_uuid(uint16_t conn_handle, uint16_t start_handle,
                           uint16_t end_handle, uint8_t *uuid128)
{
    intptr_t svc_start_handle;
    int rc;

    svc_start_handle = start_handle;
    rc = ble_gattc_disc_chrs_by_uuid(conn_handle, start_handle, end_handle,
                                     uuid128, bleshell_on_disc_c,
                                     &svc_start_handle);
    return rc;
}

int
bleshell_disc_svcs(uint16_t conn_handle)
{
    int rc;

    rc = ble_gattc_disc_all_svcs(conn_handle, bleshell_on_disc_s, NULL);
    return rc;
}

int
bleshell_disc_svc_by_uuid(uint16_t conn_handle, uint8_t *uuid128)
{
    int rc;

    rc = ble_gattc_disc_svc_by_uuid(conn_handle, uuid128,
                                    bleshell_on_disc_s, NULL);
    return rc;
}

int
bleshell_disc_all_dscs(uint16_t conn_handle, uint16_t chr_def_handle,
                       uint16_t chr_end_handle)
{
    intptr_t chr_def_handle_iptr;
    int rc;

    chr_def_handle_iptr = chr_def_handle;
    rc = ble_gattc_disc_all_dscs(conn_handle, chr_def_handle, chr_end_handle,
                                 bleshell_on_disc_d, &chr_def_handle_iptr);
    return rc;
}

int
bleshell_find_inc_svcs(uint16_t conn_handle, uint16_t start_handle,
                       uint16_t end_handle)
{
    int rc;

    rc = ble_gattc_find_inc_svcs(conn_handle, start_handle, end_handle,
                                 bleshell_on_disc_s, NULL);
    return rc;
}

int
bleshell_read(uint16_t conn_handle, uint16_t attr_handle)
{
    int rc;

    rc = ble_gattc_read(conn_handle, attr_handle, bleshell_on_read, NULL);
    return rc;
}

int
bleshell_read_long(uint16_t conn_handle, uint16_t attr_handle)
{
    int rc;

    rc = ble_gattc_read_long(conn_handle, attr_handle, bleshell_on_read, NULL);
    return rc;
}

int
bleshell_read_by_uuid(uint16_t conn_handle, uint16_t start_handle,
                      uint16_t end_handle, uint8_t *uuid128)
{
    int rc;

    rc = ble_gattc_read_by_uuid(conn_handle, start_handle, end_handle, uuid128,
                                bleshell_on_read, NULL);
    return rc;
}

int
bleshell_read_mult(uint16_t conn_handle, uint16_t *attr_handles,
                   int num_attr_handles)
{
    int rc;

    rc = ble_gattc_read_mult(conn_handle, attr_handles, num_attr_handles,
                             bleshell_on_read_mult, NULL);
    return rc;
}

int
bleshell_write(uint16_t conn_handle, uint16_t attr_handle, void *value,
               uint16_t value_len)
{
    int rc;

    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        rc = ble_att_svr_write_local(attr_handle, value, value_len);
    } else {
        rc = ble_gattc_write(conn_handle, attr_handle, value, value_len,
                             bleshell_on_write, NULL);
    }

    return rc;
}

int
bleshell_write_no_rsp(uint16_t conn_handle, uint16_t attr_handle, void *value,
                      uint16_t value_len)
{
    int rc;

    rc = ble_gattc_write_no_rsp(conn_handle, attr_handle, value, value_len,
                                bleshell_on_write, NULL);
    return rc;
}

int
bleshell_write_long(uint16_t conn_handle, uint16_t attr_handle, void *value,
                    uint16_t value_len)
{
    int rc;

    rc = ble_gattc_write_long(conn_handle, attr_handle, value, value_len,
                              bleshell_on_write, NULL);
    return rc;
}

int
bleshell_write_reliable(uint16_t conn_handle, struct ble_gatt_attr *attrs,
                        int num_attrs)
{
    int rc;

    rc = ble_gattc_write_reliable(conn_handle, attrs, num_attrs,
                                  bleshell_on_write_reliable, NULL);
    return rc;
}

int
bleshell_adv_stop(void)
{
    int rc;

    rc = ble_gap_conn_adv_stop();
    return rc;
}

int
bleshell_adv_start(int disc, int conn, uint8_t *peer_addr, int addr_type)
{
    int rc;

    rc = ble_gap_conn_adv_start(disc, conn, peer_addr, addr_type,
                                bleshell_on_connect, NULL);
    return rc;
}

int
bleshell_conn_initiate(int addr_type, uint8_t *peer_addr,
                       struct ble_gap_conn_crt_params *params)
{
    int rc;

    rc = ble_gap_conn_initiate(addr_type, peer_addr, NULL, bleshell_on_connect,
                               NULL);
    return rc;
}

int
bleshell_conn_cancel(void)
{
    int rc;

    rc = ble_gap_conn_cancel();
    return rc;
}

int
bleshell_term_conn(uint16_t conn_handle)
{
    int rc;

    rc = ble_gap_conn_terminate(conn_handle);
    return rc;
}

int
bleshell_wl_set(struct ble_gap_white_entry *white_list, int white_list_count)
{
    int rc;

    rc = ble_gap_conn_wl_set(white_list, white_list_count, bleshell_on_wl_set,
                             NULL);
    return rc;
}

int
bleshell_scan(uint32_t dur_ms, uint8_t disc_mode, uint8_t scan_type,
              uint8_t filter_policy)
{
    int rc;

    rc = ble_gap_conn_disc(dur_ms, disc_mode, scan_type, filter_policy,
                           bleshell_on_scan, NULL);
    return rc;
}

int
bleshell_set_adv_data(struct ble_hs_adv_fields *adv_fields)
{
    int rc;

    rc = ble_gap_conn_set_adv_fields(adv_fields);
    return rc;
}

int
bleshell_update_conn(uint16_t conn_handle,
                     struct ble_gap_conn_upd_params *params)
{
    int rc;

    rc = ble_gap_conn_update_params(conn_handle, params);
    return rc;
}

/**
 * BLE test task 
 * 
 * @param arg 
 */
static void
bleshell_task_handler(void *arg)
{
    struct os_event *ev;
    struct os_callout_func *cf;

    periph_init();

    ble_att_set_notify_cb(bleshell_on_notify, NULL);

    /* Initialize eventq */
    os_eventq_init(&g_bleshell_evq);

    /* Init bleshell variables */
    g_bleshell_state = 0;
    g_next_os_time = os_time_get();
    
    while (1) {
        ev = os_eventq_get(&g_bleshell_evq);
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
    int i;
    int rc;
    uint32_t seed;

    /* Initialize OS */
    os_init();

    /* Set cputime to count at 1 usec increments */
    rc = cputime_init(1000000);
    assert(rc == 0);

    rc = os_mempool_init(&g_mbuf_mempool, MBUF_NUM_MBUFS, 
            MBUF_MEMBLOCK_SIZE, &g_mbuf_buffer[0], "mbuf_pool");

    rc = os_mbuf_pool_init(&g_mbuf_pool, &g_mbuf_mempool, MBUF_MEMBLOCK_SIZE, 
                           MBUF_NUM_MBUFS);
    assert(rc == 0);

    /* Dummy device address */
    memcpy(g_dev_addr, bleshell_addr, 6);

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
    gpio_init_out(g_led_pin, 1);

    bleshell_svc_mem = malloc(
        OS_MEMPOOL_BYTES(BLESHELL_MAX_SVCS, sizeof (struct bleshell_svc)));
    assert(bleshell_svc_mem != NULL);

    rc = os_mempool_init(&bleshell_svc_pool, BLESHELL_MAX_SVCS,
                         sizeof (struct bleshell_svc), bleshell_svc_mem,
                         "bleshell_svc_pool");
    assert(rc == 0);

    bleshell_chr_mem = malloc(
        OS_MEMPOOL_BYTES(BLESHELL_MAX_CHRS, sizeof (struct bleshell_chr)));
    assert(bleshell_chr_mem != NULL);

    rc = os_mempool_init(&bleshell_chr_pool, BLESHELL_MAX_CHRS,
                         sizeof (struct bleshell_chr), bleshell_chr_mem,
                         "bleshell_chr_pool");
    assert(rc == 0);

    bleshell_dsc_mem = malloc(
        OS_MEMPOOL_BYTES(BLESHELL_MAX_DSCS, sizeof (struct bleshell_dsc)));
    assert(bleshell_dsc_mem != NULL);

    rc = os_mempool_init(&bleshell_dsc_pool, BLESHELL_MAX_DSCS,
                         sizeof (struct bleshell_dsc), bleshell_dsc_mem,
                         "bleshell_dsc_pool");
    assert(rc == 0);

    os_task_init(&bleshell_task, "bleshell", bleshell_task_handler,
                 NULL, BLESHELL_TASK_PRIO, OS_WAIT_FOREVER,
                 bleshell_stack, BLESHELL_STACK_SIZE);

    /* Initialize host HCI */
    rc = ble_hs_init(HOST_TASK_PRIO);
    assert(rc == 0);

    /* Initialize the BLE LL */
    ble_ll_init();

    rc = shell_task_init(SHELL_TASK_PRIO, shell_stack, SHELL_TASK_STACK_SIZE);
    assert(rc == 0);

    /* Init the console */
    rc = console_init(shell_console_rx_cb);
    assert(rc == 0);

    rc = cmd_init();
    assert(rc == 0);

    /* Initialize the preferred parameters. */
    htole16(bleshell_pref_conn_params + 0, BLE_GAP_INITIAL_CONN_ITVL_MIN);
    htole16(bleshell_pref_conn_params + 2, BLE_GAP_INITIAL_CONN_ITVL_MAX);
    htole16(bleshell_pref_conn_params + 4, 0);
    htole16(bleshell_pref_conn_params + 6, BSWAP16(0x100));

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return 0;
}

