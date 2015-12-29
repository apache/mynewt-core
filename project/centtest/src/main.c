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

static uint8_t centtest_slv_addr[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
static uint8_t centtest_mst_addr[6] = {0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a};

/* Create a mbuf pool of BLE mbufs */
#define MBUF_NUM_MBUFS      (8)
#define MBUF_BUF_SIZE       (256 + sizeof(struct hci_data_hdr))
#define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + BLE_MBUF_PKT_OVERHEAD)

#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

struct os_mbuf_pool g_mbuf_pool; 
struct os_mempool g_mbuf_mempool;
os_membuf_t g_mbuf_buffer[MBUF_MEMPOOL_SIZE];

/* CENTTEST variables */
#define CENTTEST_STACK_SIZE              (256)
#define CENTTEST_TASK_PRIO               (HOST_TASK_PRIO + 1)
uint32_t g_next_os_time;
int g_centtest_state;
struct os_eventq g_centtest_evq;
struct os_task centtest_task;
os_stack_t centtest_stack[CENTTEST_STACK_SIZE];

void
bletest_inc_adv_pkt_num(void) { }


static int
centtest_on_read(uint16_t conn_handle, int status,
                     struct ble_gatt_attr *attr, void *arg)
{
    uint8_t *u8ptr;
    int i;

    if (status != 0) {
        console_printf("characteristic read failure: status=%d\n", status);
        return 0;
    }

    console_printf("characteristic read: handle=%d value=", attr->handle);
    u8ptr = attr->value;
    for (i = 0; i < attr->value_len; i++) {
        if (i != 0) {
            console_printf(":");
        }
        console_printf("%02x", u8ptr[i]);
    }
    console_printf("\n");

    return 0;
}

static int
centtest_on_disc_c(uint16_t conn_handle, int status,
                       struct ble_gatt_chr *chr, void *arg)
{
    int rc;
    int i;

    if (status != 0) {
        console_printf("characteristic discovery failure: status=%d\n",
                       status);
        return 0;
    }

    if (chr == NULL) {
        console_printf("characteristic discovery complete.\n");
        return 0;
    }

    console_printf("characteristic discovered: decl_handle=%d value_handle=%d "
                   "properties=%d uuid=", chr->decl_handle, chr->value_handle,
                   chr->properties);
    for (i = 0; i < 16; i++) {
        if (i != 0) {
            console_printf(":");
        }
        console_printf("%02x", chr->uuid128[i]);
    }
    console_printf("\n");

    rc = ble_gatt_read(conn_handle, chr->value_handle, centtest_on_read, NULL);
    assert(rc == 0);

    return 0;
}

static int
centtest_on_disc_s(uint16_t conn_handle, int status,
                       struct ble_gatt_service *service, void *arg)
{
    int i;

    if (status != 0) {
        console_printf("service discovery failure: status=%d\n", status);
        return 0;
    }

    if (service == NULL) {
        console_printf("service discovery complete.\n");
        return 0;
    }

    console_printf("service discovered: start_handle=%d end_handle=%d, uuid=",
                   service->start_handle, service->end_handle);
    for (i = 0; i < 16; i++) {
        if (i != 0) {
            console_printf(":");
        }
        console_printf("%02x", service->uuid128[i]);
    }
    console_printf("\n");

    ble_gatt_disc_all_chars(conn_handle, service->start_handle,
                            service->end_handle, centtest_on_disc_c,
                            NULL);

    return 0;
}

static void
centtest_print_adv_rpt(struct ble_gap_conn_adv_rpt *adv)
{
    console_printf("Received advertisement report:\n");
    console_printf("    addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
                   adv->addr[0], adv->addr[1], adv->addr[2],
                   adv->addr[3], adv->addr[4], adv->addr[5]);
    console_printf("    flags=0x%02x\n", adv->fields.flags);
    console_printf("    name=");
    console_write((char *)adv->fields.name, adv->fields.name_len);
    console_printf("%s\n");
}

static void
centtest_on_connect(struct ble_gap_conn_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_CONN_EVENT_TYPE_CONNECT:
        console_printf("connection complete; handle=%d status=%d "
                       "peer_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
                       event->conn.handle, event->conn.status,
                       event->conn.peer_addr[0], event->conn.peer_addr[1],
                       event->conn.peer_addr[2], event->conn.peer_addr[3],
                       event->conn.peer_addr[4], event->conn.peer_addr[5]);

        if (event->conn.status == 0) {
            ble_gatt_disc_all_services(event->conn.handle,
                                       centtest_on_disc_s, NULL);
        }

        break;

    case BLE_GAP_CONN_EVENT_TYPE_ADV_RPT:
        centtest_print_adv_rpt(&event->adv);
        break;

    case BLE_GAP_CONN_EVENT_TYPE_SCAN_DONE:
        console_printf("scan complete\n");
        break;
    }
}

/**
 * BLE test task 
 * 
 * @param arg 
 */
void
centtest_task_handler(void *arg)
{
    struct os_event *ev;
    struct os_callout_func *cf;
    int rc;

    /* We are initialized */
    console_printf("INITIATOR\n");

    /* Initialize eventq */
    os_eventq_init(&g_centtest_evq);

    /* Init centtest variables */
    g_centtest_state = 0;
    g_next_os_time = os_time_get();
    
    ble_gap_conn_set_cb(centtest_on_connect, NULL);

    //rc = ble_gap_conn_disc(20000, BLE_GAP_DISC_MODE_GEN);
    rc = ble_gap_conn_direct_connect(BLE_HCI_ADV_PEER_ADDR_PUBLIC,
                                     centtest_slv_addr);
    assert(rc == 0);

    while (1) {
        ev = os_eventq_get(&g_centtest_evq);
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

    os_task_init(&centtest_task, "centtest", centtest_task_handler,
                 NULL, CENTTEST_TASK_PRIO, OS_WAIT_FOREVER,
                 centtest_stack, CENTTEST_STACK_SIZE);

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
    memcpy(g_dev_addr, centtest_mst_addr, 6);

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

