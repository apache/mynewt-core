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
#include "host/ble_uuid.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "controller/ble_ll.h"

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
#define BLESHELL_MAX_CHRS               8

uint32_t g_next_os_time;
int g_bleshell_state;
struct os_eventq g_bleshell_evq;
struct os_task bleshell_task;
os_stack_t bleshell_stack[BLESHELL_STACK_SIZE];

struct bleshell_conn ble_shell_conns[BLESHELL_MAX_CONNS];
int bleshell_num_conns;

void
bletest_inc_adv_pkt_num(void) { }

struct bleshell_conn bleshell_conns[BLESHELL_MAX_CONNS];
int bleshell_num_conns;

static void *bleshell_svc_mem;
static struct os_mempool bleshell_svc_pool;

static void *bleshell_chr_mem;
static struct os_mempool bleshell_chr_pool;

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
    STAILQ_INIT(&conn->svcs);

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
    while ((svc = STAILQ_FIRST(&conn->svcs)) != NULL) {
        STAILQ_REMOVE_HEAD(&conn->svcs, next);
        os_memblock_put(&bleshell_svc_pool, svc);
    }

    bleshell_num_conns--;
    for (i = idx; i < bleshell_num_conns; i++) {
        bleshell_conns[i - 1] = bleshell_conns[i];
    }
}

static struct bleshell_svc *
bleshell_svc_add(uint16_t conn_handle, struct ble_gatt_service *gatt_svc)
{
    struct bleshell_conn *conn;
    struct bleshell_svc *svc;

    conn = bleshell_conn_find(conn_handle);
    if (conn == NULL) {
        console_printf("RECEIVED SERVICE FOR UNKNOWN CONNECTION; HANDLE=%d\n",
                       conn_handle);
        return NULL;
    }

    svc = os_memblock_get(&bleshell_svc_pool);
    if (svc == NULL) {
        console_printf("OOM WHILE DISCOVERING SERVICE\n");
        return NULL;
    }

    svc->svc = *gatt_svc;
    STAILQ_INIT(&svc->chrs);

    STAILQ_INSERT_TAIL(&conn->svcs, svc, next);

    return svc;
}

static struct bleshell_svc *
bleshell_svc_find(struct bleshell_conn *conn, uint16_t svc_start_handle)
{
    struct bleshell_svc *svc;

    STAILQ_FOREACH(svc, &conn->svcs, next) {
        if (svc->svc.start_handle == svc_start_handle) {
            return svc;
        }
    }

    return NULL;
}

static struct bleshell_chr *
bleshell_chr_add(uint16_t conn_handle,  uint16_t svc_start_handle,
                 struct ble_gatt_chr *gatt_chr)
{
    struct bleshell_conn *conn;
    struct bleshell_svc *svc;
    struct bleshell_chr *chr;

    conn = bleshell_conn_find(conn_handle);
    if (conn == NULL) {
        console_printf("RECEIVED SERVICE FOR UNKNOWN CONNECTION; HANDLE=%d\n",
                       conn_handle);
        return NULL;
    }

    svc = bleshell_svc_find(conn, svc_start_handle);
    if (svc == NULL) {
        console_printf("CAN'T FIND SERVICE FOR DISCOVERED CHR; HANDLE=%d\n",
                       conn_handle);
        return NULL;
    }

    chr = os_memblock_get(&bleshell_chr_pool);
    if (chr == NULL) {
        console_printf("OOM WHILE DISCOVERING CHARACTERISTIC\n");
        return NULL;
    }

    chr->chr = *gatt_chr;
    STAILQ_INSERT_TAIL(&svc->chrs, chr, next);

    return chr;
}

static int
bleshell_on_disc_s(uint16_t conn_handle, struct ble_gatt_error *error,
                   struct ble_gatt_service *service, void *arg)
{
    if (error != NULL) {
        console_printf("ERROR DISCOVERING SERVICE: conn_handle=%d status=%d "
                       "att_handle=%d\n", conn_handle, error->status,
                       error->att_handle);
    } else if (service != NULL) {
        bleshell_svc_add(conn_handle, service);
    } else {
        /* Service discovery complete. */
    }

    return 0;
}

int
bleshell_on_disc_c(uint16_t conn_handle, struct ble_gatt_error *error,
                   struct ble_gatt_chr *chr, void *arg)
{
    intmax_t *svc_start_handle;

    svc_start_handle = arg;

    if (error != NULL) {
        console_printf("ERROR DISCOVERING CHARACTERISTIC: conn_handle=%d "
                       "status=%d att_handle=%d\n",
                       conn_handle, error->status, error->att_handle);
    } else if (chr != NULL) {
        bleshell_chr_add(conn_handle, *svc_start_handle, chr);
    } else {
        /* Service discovery complete. */
    }

    return 0;
}

static void
bleshell_on_connect(int event, int status, struct ble_gap_conn_desc *desc,
                    void *arg)
{
    int conn_idx;

    switch (event) {
    case BLE_GAP_EVENT_CONN:
        console_printf("connection complete; handle=%d status=%d "
                       "peer_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
                       desc->conn_handle, status,
                       desc->peer_addr[0], desc->peer_addr[1],
                       desc->peer_addr[2], desc->peer_addr[3],
                       desc->peer_addr[4], desc->peer_addr[5]);

        if (status == 0) {
            bleshell_conn_add(desc);
        } else {
            conn_idx = bleshell_conn_find_idx(desc->conn_handle);
            if (conn_idx == -1) {
                console_printf("UNKNOWN CONNECTION\n");
            } else {
                bleshell_conn_delete_idx(conn_idx);
            }
        }

        break;
    }
}

int
bleshell_disc_all_chrs(uint16_t conn_handle, uint16_t start_handle,
                       uint16_t end_handle)
{
    intmax_t svc_start_handle;
    int rc;

    svc_start_handle = start_handle;
    rc = ble_gattc_disc_all_chrs(conn_handle, start_handle, end_handle,
                                 bleshell_on_disc_c, &svc_start_handle);
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
bleshell_conn_initiate(int addr_type, uint8_t *peer_addr)
{
    int rc;

    rc = ble_gap_conn_initiate(addr_type, peer_addr, bleshell_on_connect,
                               NULL);
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

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return 0;
}

