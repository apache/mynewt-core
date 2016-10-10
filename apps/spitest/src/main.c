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
#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_spi.h"
#include "stats/stats.h"
#include "config/config.h"
#include <os/os_dev.h>
#include <assert.h>
#include <string.h>
#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif
#ifdef NRF51
#include "app_util_platform.h"
#include "app_error.h"
#endif

/* The spi txrx callback */
struct sblinky_spi_cb_arg
{
    int transfers;
    int txlen;
    uint32_t tx_rx_bytes;
};
struct sblinky_spi_cb_arg spi_cb_obj;
void *spi_cb_arg;

/* Init all tasks */
volatile int tasks_initialized;
int init_tasks(void);

/* Task 1 */
#define TASK1_PRIO (1)
#define TASK1_STACK_SIZE    OS_STACK_ALIGN(1024)
struct os_task task1;
os_stack_t stack1[TASK1_STACK_SIZE];

/* Global test semaphore */
struct os_sem g_test_sem;

/* For LED toggling */
int g_led_pin;

#if MYNEWT_VAL(SPI_MASTER)
uint8_t g_spi_tx_buf[32];
uint8_t g_spi_rx_buf[32];
uint32_t g_spi_xfr_num;
uint8_t g_spi_null_rx;

void
sblinky_spi_irqm_handler(void *arg, int len)
{
    int i;
    uint8_t expval;
    struct sblinky_spi_cb_arg *cb;

    hal_gpio_set(SPI_SS_PIN);

    assert(arg == spi_cb_arg);
    if (spi_cb_arg) {
        cb = (struct sblinky_spi_cb_arg *)arg;
        assert(len == cb->txlen);
        ++cb->transfers;
    }

    /* Make sure we get back the data we expect! */
    if (g_spi_xfr_num == 1) {
        /* The first time we expect entire buffer to be filled with 0x88 */
        for (i = 0; i < len; ++i) {
            if (g_spi_rx_buf[i] != 0x88) {
                assert(0);
            }
        }
    } else {
        /*
         * We expect the buffer to be filled with an incrementing pattern
         * That starts at 31 less than the first value sent this time.
         */
        if (g_spi_null_rx == 0) {
            expval = g_spi_tx_buf[0];
            expval = (uint8_t)(expval - 31);
            for (i = 0; i < len; ++i) {
                if (g_spi_rx_buf[i] != expval) {
                    assert(0);
                }
                ++expval;
            }
        }
    }
    ++g_spi_xfr_num;
}

void
sblinky_spi_cfg(int spi_num)
{
    int spi_id;
    struct hal_spi_settings my_spi;

    my_spi.data_order = HAL_SPI_MSB_FIRST;
    my_spi.data_mode = HAL_SPI_MODE0;
    my_spi.baudrate = 8000;
    my_spi.word_size = HAL_SPI_WORD_SIZE_8BIT;
    spi_id = 0;
    hal_spi_config(spi_id, &my_spi);
}
#endif

#if MYNEWT_VAL(SPI_SLAVE)
uint8_t g_spi_tx_buf[32];
uint8_t g_spi_rx_buf[32];
uint32_t g_spi_xfr_num;

/* XXX: This is an ugly hack for now. */
#ifdef NRF51
#define SPI_SLAVE_ID    (1)
#else
#define SPI_SLAVE_ID    (0)
#endif

void
sblinky_spi_irqs_handler(void *arg, int len)
{
    struct sblinky_spi_cb_arg *cb;

    assert(arg == spi_cb_arg);
    if (spi_cb_arg) {
        cb = (struct sblinky_spi_cb_arg *)arg;
        ++cb->transfers;
        cb->tx_rx_bytes += len;
    }

    /* Post semaphore to task waiting for SPI slave */
    os_sem_release(&g_test_sem);
}

void
sblinky_spi_cfg(int spi_num)
{
    int spi_id;
    struct hal_spi_settings my_spi;

    my_spi.data_order = HAL_SPI_MSB_FIRST;
    my_spi.data_mode = HAL_SPI_MODE0;
    my_spi.baudrate = 0;
    my_spi.word_size = HAL_SPI_WORD_SIZE_8BIT;
    spi_id = SPI_SLAVE_ID;
    hal_spi_config(spi_id, &my_spi);
    hal_spi_set_txrx_cb(spi_num, sblinky_spi_irqs_handler, spi_cb_arg);
}
#endif

#if MYNEWT_VAL(SPI_MASTER)
void
task1_handler(void *arg)
{
    int i;
    int rc;
    uint16_t rxval;
    uint8_t last_val;
    uint8_t spi_nb_cntr;
    uint8_t spi_b_cntr;

    /* Set the led pin for the E407 devboard */
    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);

    /* Use SS pin for testing */
    hal_gpio_init_out(SPI_SS_PIN, 1);
    sblinky_spi_cfg(0);
    hal_spi_set_txrx_cb(0, NULL, NULL);
    hal_spi_enable(0);

    /*
     * Send some bytes in a non-blocking manner to SPI using tx val. The
     * slave should send back 0x77.
     */
    g_spi_tx_buf[0] = 0xde;
    g_spi_tx_buf[1] = 0xad;
    g_spi_tx_buf[2] = 0xbe;
    g_spi_tx_buf[3] = 0xef;
    hal_gpio_clear(SPI_SS_PIN);
    for (i = 0; i < 4; ++i) {
        rxval = hal_spi_tx_val(0, g_spi_tx_buf[i]);
        assert(rxval == 0x77);
        g_spi_rx_buf[i] = (uint8_t)rxval;
    }
    hal_gpio_set(SPI_SS_PIN);
    ++g_spi_xfr_num;

    /* Set up the callback to use when non-blocking API used */
    hal_spi_disable(0);
    spi_cb_arg = &spi_cb_obj;
    spi_cb_obj.txlen = 32;
    hal_spi_set_txrx_cb(0, sblinky_spi_irqm_handler, spi_cb_arg);
    hal_spi_enable(0);
    spi_nb_cntr = 0;
    spi_b_cntr = 0;

    while (1) {
        /* Wait one second */
        os_time_delay(OS_TICKS_PER_SEC);

        /* Toggle the LED */
        hal_gpio_toggle(g_led_pin);

        /*
         * Send to a slave, using blocking and non-blocking calls. The
         * slave should be sending back the last thing we sent it.
         */
        last_val = g_spi_tx_buf[31];
        for (i = 0; i < 32; ++i) {
            g_spi_tx_buf[i] = (uint8_t)(last_val + i);
        }

        if (g_spi_xfr_num & 1) {
            /* Send non-blocking */
            ++spi_nb_cntr;
            assert(hal_gpio_read(SPI_SS_PIN) == 1);
            hal_gpio_clear(SPI_SS_PIN);
            if (spi_nb_cntr == 7) {
                g_spi_null_rx = 1;
                rc = hal_spi_txrx_noblock(0, g_spi_tx_buf, NULL, 32);
            } else {
                g_spi_null_rx = 0;
                rc = hal_spi_txrx_noblock(0, g_spi_tx_buf, g_spi_rx_buf, 32);
            }
            assert(!rc);
        } else {
            /* Send blocking */
            ++spi_b_cntr;
            assert(hal_gpio_read(SPI_SS_PIN) == 1);
            hal_gpio_clear(SPI_SS_PIN);
            if (spi_b_cntr == 7) {
                g_spi_null_rx = 1;
                rc = hal_spi_txrx(0, g_spi_tx_buf, NULL, 32);
                spi_b_cntr = 0;
            } else {
                g_spi_null_rx = 0;
                rc = hal_spi_txrx(0, g_spi_tx_buf, g_spi_rx_buf, 32);
            }
            assert(!rc);
            hal_gpio_set(SPI_SS_PIN);
        }
    }
}
#endif

#if MYNEWT_VAL(SPI_SLAVE)
void
task1_handler(void *arg)
{
    int rc;

    /* Set the led pin for the E407 devboard */
    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);

    sblinky_spi_cfg(SPI_SLAVE_ID);
    hal_spi_enable(SPI_SLAVE_ID);

    /* Make the default character 0x77 */
    hal_spi_slave_set_def_tx_val(SPI_SLAVE_ID, 0x77);

    /*
     * Fill buffer with 0x77 for first transfer. This should be a 0xdeadbeef
     * transfer from master to start things off
     */
    memset(g_spi_tx_buf, 0x77, 32);
    rc = hal_spi_txrx_noblock(SPI_SLAVE_ID, g_spi_tx_buf, g_spi_rx_buf,
                              32);

    while (1) {
        /* Wait for semaphore from ISR */
        os_sem_pend(&g_test_sem, OS_TIMEOUT_NEVER);

        if (g_spi_xfr_num == 0) {
            /* Since we dont know what master will send, we fill 0x88 */
            memset(g_spi_tx_buf, 0x88, 32);
            rc = hal_spi_txrx_noblock(SPI_SLAVE_ID, g_spi_tx_buf, g_spi_rx_buf,
                                      32);
        } else {
            /* transmit back what we just received */
            memcpy(g_spi_tx_buf, g_spi_rx_buf, 32);
            rc = hal_spi_txrx_noblock(SPI_SLAVE_ID, g_spi_tx_buf, g_spi_rx_buf,
                                      32);
            assert(rc == 0);
        }
        ++g_spi_xfr_num;

        /* Toggle the LED */
        hal_gpio_toggle(g_led_pin);
    }
}
#endif

/**
 * init_tasks
 *
 * Called by main.c after sysinit(). This function performs initializations
 * that are required before tasks are running.
 *
 * @return int 0 success; error otherwise.
 */
int
init_tasks(void)
{
    /* Initialize global test semaphore */
    os_sem_init(&g_test_sem, 0);

    os_task_init(&task1, "task1", task1_handler, NULL,
            TASK1_PRIO, OS_WAIT_FOREVER, stack1, TASK1_STACK_SIZE);

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
main(int argc, char **argv)
{
    int rc;

#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif

    sysinit();
    rc = init_tasks();
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return rc;
}

