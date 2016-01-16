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
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_cputime.h"
#include "console/console.h"

/* BLE */
#include "nimble/ble.h"
#include "host/host_hci.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "controller/ble_ll.h"

/* Init all tasks */
volatile int tasks_initialized;

/* Task 1 */
#define HOST_TASK_PRIO      (1)

/* For LED toggling */
int g_led_pin;

/* Our global device address (public) */
uint8_t g_dev_addr[BLE_DEV_ADDR_LEN];

/* Our random address (in case we need it) */
uint8_t g_random_addr[BLE_DEV_ADDR_LEN];

/* A buffer for host advertising data */
uint8_t g_host_adv_data[BLE_HCI_MAX_ADV_DATA_LEN];
uint8_t g_host_adv_len;

static uint8_t prphtest_slv_addr[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

/* Create a mbuf pool of BLE mbufs */
#define MBUF_NUM_MBUFS      (8)
#define MBUF_BUF_SIZE       (256 + sizeof(struct hci_data_hdr))
#define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + BLE_MBUF_PKT_OVERHEAD)

#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

struct os_mbuf_pool g_mbuf_pool; 
struct os_mempool g_mbuf_mempool;
os_membuf_t g_mbuf_buffer[MBUF_MEMPOOL_SIZE];

/* PRPHTEST variables */
#define PRPHTEST_STACK_SIZE              (256)
#define PRPHTEST_TASK_PRIO               (HOST_TASK_PRIO + 1)
uint32_t g_next_os_time;
int g_prphtest_state;
struct os_eventq g_prphtest_evq;
struct os_task prphtest_task;
os_stack_t prphtest_stack[PRPHTEST_STACK_SIZE];

void
bletest_inc_adv_pkt_num(void) { }

static int
prphtest_gatt_cb(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
                 union ble_gatt_access_ctxt *ctxt, void *arg);

#define PRPHTEST_SVC1_UUID      0x1234
#define PRPHTEST_SVC2_UUID      0x5678
#define PRPHTEST_CHR1_UUID      0x1111
#define PRPHTEST_CHR2_UUID      0x1112
#define PRPHTEST_CHR3_UUID      0x5555

static const struct ble_gatt_svc_def prphtest_svcs[] = { {
    /*** Service 0x1234. */
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid128 = BLE_UUID16(PRPHTEST_SVC1_UUID),
    .characteristics = (struct ble_gatt_chr_def[]) { {
        /*** Characteristic 0x1111. */
        .uuid128 = BLE_UUID16(PRPHTEST_CHR1_UUID),
        .access_cb = prphtest_gatt_cb,
        .flags = 0,
    }, {
        /*** Characteristic 0x1112. */
        .uuid128 = BLE_UUID16(PRPHTEST_CHR2_UUID),
        .access_cb = prphtest_gatt_cb,
        .flags = 0,
    }, {
        .uuid128 = NULL, /* No more characteristics in this service. */
    } },
}, {
    /*** Service 0x5678. */
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid128 = BLE_UUID16(PRPHTEST_SVC2_UUID),
    .characteristics = (struct ble_gatt_chr_def[]) { {
        /*** Characteristic 0x5555. */
        .uuid128 = BLE_UUID16(PRPHTEST_CHR3_UUID),
        .access_cb = prphtest_gatt_cb,
        .flags = 0,
    }, {
        .uuid128 = NULL, /* No more characteristics in this service. */
    } },
}, {
    .type = BLE_GATT_SVC_TYPE_END, /* No more services. */
}, };

static int
prphtest_gatt_cb(uint16_t conn_handle, uint16_t attr_handle, uint8_t op,
                 union ble_gatt_access_ctxt *ctxt, void *arg)
{
    static uint8_t buf[128];
    uint16_t uuid16;

    assert(op == BLE_GATT_ACCESS_OP_READ_CHR);

    uuid16 = ble_uuid_128_to_16(ctxt->chr_access.chr->uuid128);
    switch (uuid16) {
    case PRPHTEST_CHR1_UUID:
        console_printf("reading characteristic1 value");
        memcpy(buf, "char1", 5);
        ctxt->chr_access.len = 5;
        break;

    case PRPHTEST_CHR2_UUID:
        console_printf("reading characteristic2 value");
        memcpy(buf, "char2", 5);
        ctxt->chr_access.len = 5;
        break;

    case PRPHTEST_CHR3_UUID:
        console_printf("reading characteristic3 value");
        memcpy(buf, "char3", 5);
        ctxt->chr_access.len = 5;
        break;

    default:
        assert(0);
        break;
    }

    ctxt->chr_access.data = buf;

    return 0;
}

static void
prphtest_register_cb(uint8_t op, union ble_gatt_register_ctxt *ctxt, void *arg)
{
    uint16_t uuid16;

    switch (op) {
    case BLE_GATT_REGISTER_OP_SVC:
        uuid16 = ble_uuid_128_to_16(ctxt->svc_reg.svc->uuid128);
        assert(uuid16 != 0);
        console_printf("registered service 0x%04x with handle=%d\n",
                       uuid16, ctxt->svc_reg.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        uuid16 = ble_uuid_128_to_16(ctxt->chr_reg.chr->uuid128);
        assert(uuid16 != 0);
        console_printf("registering characteristic 0x%04x with def_handle=%d "
                       "val_handle=%d\n",
                       uuid16, ctxt->chr_reg.def_handle,
                       ctxt->chr_reg.val_handle);
        break;

    default:
        assert(0);
        break;
    }
}

static void
prphtest_register_attrs(void)
{
    int rc;

    rc = ble_gatts_register_svcs(prphtest_svcs, prphtest_register_cb, NULL);
    assert(rc == 0);
}

static void
prphtest_on_connect(int event, int status, struct ble_gap_conn_desc *desc,
                    void *arg)
{
    switch (event) {
    case BLE_GAP_EVENT_CONN:
        console_printf("connection event; handle=%d status=%d "
                       "peer_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
                       desc->conn_handle, status,
                       desc->peer_addr[0], desc->peer_addr[1],
                       desc->peer_addr[2], desc->peer_addr[3],
                       desc->peer_addr[4], desc->peer_addr[5]);
        break;

    default:
        console_printf("unexpected connection event; type=%d\n", event);
        break;
    }
}

/**
 * BLE test task 
 * 
 * @param arg 
 */
void
prphtest_task_handler(void *arg)
{
    struct ble_hs_adv_fields fields;
    struct os_callout_func *cf;
    struct os_event *ev;
    int rc;

    /* We are initialized */
    console_printf("ADVERTISER\n");

    /* Initialize eventq */
    os_eventq_init(&g_prphtest_evq);

    /* Init prphtest variables */
    g_prphtest_state = 0;
    g_next_os_time = os_time_get();
    
    prphtest_register_attrs();

    memset(&fields, 0, sizeof fields);
    fields.name = (uint8_t *)"nimble";
    fields.name_len = 6;
    fields.name_is_complete = 1;

    fields.uuids16 = (uint16_t[]) { PRPHTEST_SVC1_UUID, PRPHTEST_SVC2_UUID };
    fields.num_uuids16 = 2;
    fields.uuids16_is_complete = 1;

    fields.le_role = 0;
    fields.le_role_is_present = 1;

    rc = ble_gap_conn_set_adv_fields(&fields);
    assert(rc == 0);

    rc = ble_gap_conn_adv_start(BLE_GAP_DISC_MODE_GEN, BLE_GAP_CONN_MODE_UND,
                                NULL, 0, prphtest_on_connect, NULL);
    assert(rc == 0);

    while (1) {
        ev = os_eventq_get(&g_prphtest_evq);
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

    os_task_init(&prphtest_task, "prphtest", prphtest_task_handler,
                 NULL, PRPHTEST_TASK_PRIO, OS_WAIT_FOREVER,
                 prphtest_stack, PRPHTEST_STACK_SIZE);

    tasks_initialized = 1;

    /* Initialize host HCI */
    rc = ble_hs_init(HOST_TASK_PRIO);
    assert(rc == 0);

    /* Initialize the BLE LL */
    ble_ll_init();

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
    memcpy(g_dev_addr, prphtest_slv_addr, 6);

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

    /* Init the console */
    rc = console_init(NULL);
    assert(rc == 0);

    /* Init tasks */
    init_tasks();

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return rc;
}
