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
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_cputime.h"
#include <assert.h>
#include <string.h>

/* BLE */
#include "nimble/ble.h"
#include "host/host_hci.h"
#include "controller/ll_adv.h"

/* Init all tasks */
volatile int tasks_initialized;
int init_tasks(void);

/* Task 1 */
#define HOST_TASK_PRIO      (1)
#define HOST_STACK_SIZE     OS_STACK_ALIGN(256)
struct os_task host_task;
os_stack_t host_stack[HOST_STACK_SIZE];
static volatile int g_host_loops;

/* For LED toggling */
int g_led_pin;

/* Our global device address (public) */
uint8_t g_dev_addr[BLE_DEV_ADDR_LEN];

/* A buffer for host advertising data */
uint8_t g_host_adv_data[BLE_ADV_DATA_MAX_LEN];

/* XXX: dont belong here */
extern int host_hci_init(void);
extern int ll_init(void);
/* XXX */

/* Create a mbuf pool */
#define MBUF_NUM_MBUFS      (16)
#define MBUF_BUF_SIZE       (256)
#define MBUF_HDR_LEN        (16)
#define MBUF_MEMBLOCK_SIZE  \
    (sizeof(struct os_mbuf) + sizeof(struct os_mbuf_pkthdr) + \
    MBUF_HDR_LEN + MBUF_BUF_SIZE)
#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

struct os_mbuf_pool g_mbuf_pool; 
struct os_mempool g_mbuf_mempool;
os_membuf_t g_mbuf_buffer[MBUF_MEMPOOL_SIZE];

/* Some application configurations */
#define BLETEST_CFG_ADV_ITVL        (2000000 / BLE_LL_ADV_ITVL)
#define BLETEST_CFG_ADV_TYPE        BLE_ADV_TYPE_ADV_NONCONN_IND

void 
host_task_handler(void *arg)
{
    int rc;
    int state;
    uint8_t adv_len;
    uint8_t *dptr;
    struct hci_adv_params adv;
    uint16_t adv_itvl;
    uint32_t next_os_adv_time;

    /* Set the led pin as an output */
    g_led_pin = LED_BLINK_PIN;
    gpio_init_out(g_led_pin, 1);

    /* Initialize host HCI */
    host_hci_init();

    /* Initialize the BLE LL */
    ll_init();

    /* Create some advertising data */
    dptr = &g_host_adv_data[0];
    dptr[0] = 0x08;
    dptr[1] = 0x09;
    dptr[2] = 'r';
    dptr[3] = 'u';
    dptr[4] = 'n';
    dptr[5] = 't';
    dptr[6] = 'i';
    dptr[7] = 'm';
    dptr[8] = 'e';
    adv_len = 9;
    dptr += 9;

    /* Add local device address */
    dptr[0] = 0x08;
    dptr[1] = 0x1B;
    dptr[2] = 0x00;
    memcpy(dptr + 3, g_dev_addr, BLE_DEV_ADDR_LEN);
    adv_len += 9;

    /* Set advertising parameters */
    adv_itvl = BLETEST_CFG_ADV_ITVL; /* Advertising interval */
    adv.adv_type = BLETEST_CFG_ADV_TYPE;
    adv.adv_channel_map = 0x07;
    adv.adv_filter_policy = BLE_ADV_FILT_NONE;
    adv.own_addr_type = BLE_ADV_OWN_ADDR_PUBLIC;
    adv.peer_addr_type = BLE_ADV_PEER_ADDR_PUBLIC;
    adv.adv_itvl_min = BLE_LL_ADV_ITVL_NONCONN_MIN;
    adv.adv_itvl_max = adv_itvl;
    rc = host_hci_cmd_le_set_adv_params(&adv);
    assert(rc == 0);

    /* Set advertising data */
    rc = host_hci_cmd_le_set_adv_data(&g_host_adv_data[0], adv_len);
    assert(rc == 0);

    /* Set rate at which we will turn on/off advertising */
    state = 0;
    next_os_adv_time = os_time_get();

    /* Sit in a loop transmitting advertising packets */
    while (1) {
        /* Number of times through the loop */
        ++g_host_loops;

        /* Wait 1 second */
        os_time_delay(OS_TICKS_PER_SEC);

        /* Toggle the LED */
        gpio_toggle(g_led_pin);

        /* Enable advertising */
        if ((int32_t)(os_time_get() - next_os_adv_time) >= 0) {
            if (state) {
                rc = host_hci_cmd_le_set_adv_enable(0);
                assert(rc == 0);
                state = 0;
            } else {
                rc = host_hci_cmd_le_set_adv_enable(1);
                assert(rc == 0);
                state = 1;
            }
            next_os_adv_time += (OS_TICKS_PER_SEC * 30);
        }

#if 0
        /* Wait for a bit */
        os_time_delay(100 * OS_TIME_TICK);

        /* Disable advertising */
        rc = host_hci_cmd_le_set_adv_enable(0);
        assert(rc == 0);
#endif
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
int
init_tasks(void)
{
    os_task_init(&host_task, "host", host_task_handler, NULL, HOST_TASK_PRIO, 
                 OS_WAIT_FOREVER, host_stack, HOST_STACK_SIZE);

    tasks_initialized = 1;

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

    rc = os_mbuf_pool_init(&g_mbuf_pool, &g_mbuf_mempool, MBUF_HDR_LEN, 
                           MBUF_MEMBLOCK_SIZE, MBUF_NUM_MBUFS);
    assert(rc == 0);

    /* Dummy device address */
    g_dev_addr[0] = 0x00;
    g_dev_addr[1] = 0x00;
    g_dev_addr[2] = 0x00;
    g_dev_addr[3] = 0x88;
    g_dev_addr[4] = 0x88;
    g_dev_addr[5] = 0x08;

    /* 
     * Seed random number generator with least significant bytes of device
     * address
     */ 
    seed = 0;
    for (i = 0; i < 4; ++i) {
        seed |= g_dev_addr[i];
        seed <<= 8;
    }
    srand(seed);

    /* Init tasks */
    init_tasks();

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return rc;
}

