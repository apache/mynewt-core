/*
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
#include <mmc/mmc.h>
#include <mcu/stm32f4xx_mynewt_hal.h>
#include <os/os_dev.h>
#include <assert.h>
#include <string.h>
#include <console/console.h>
#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
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

/* Task 1 */
#define TASK1_PRIO (1)
#define TASK1_STACK_SIZE    OS_STACK_ALIGN(1024)
struct os_task task1;

/* Task 2 */
#define TASK2_PRIO (2)
#define TASK2_STACK_SIZE    OS_STACK_ALIGN(512)
static struct os_task task2;

#define TASK3_PRIO (3)
#define TASK3_STACK_SIZE    OS_STACK_ALIGN(512)
static struct os_task task3;

static struct os_eventq mmctest_evq;

/* Global test semaphore */
struct os_sem g_test_sem;

/* For LED toggling */
int g_led_pin;

static const int SPI_SS_PIN   = MCU_GPIO_PORTA(4);
static const int SPI_SCK_PIN  = MCU_GPIO_PORTA(5);
static const int SPI_MISO_PIN = MCU_GPIO_PORTA(6);
static const int SPI_MOSI_PIN = MCU_GPIO_PORTA(7);

uint8_t g_spi_tx_buf[32];
uint8_t g_spi_last_tx_buf[32];
uint8_t g_spi_rx_buf[32];
uint32_t g_spi_xfr_num;
uint8_t g_spi_null_rx;
uint8_t g_last_tx_len;

static
void mmctest_validate_last(int len)
{
    int i;
    int curlen;
    int remlen;
    int curindex;
    uint8_t expval;

    if (g_spi_null_rx == 0) {
        expval = 0xaa;
        if (g_last_tx_len < len) {
            curlen = g_last_tx_len;
            remlen = len - g_last_tx_len;
        } else {
            curlen = len;
            remlen = 0;
        }
        for (i = 0; i < curlen; ++i) {
            if (g_spi_rx_buf[i] != g_spi_last_tx_buf[i]) {
                assert(0);
            }
        }
        curindex = curlen;
        for (i = 0; i < remlen; ++i) {
            if (g_spi_rx_buf[curindex + i] != expval) {
                assert(0);
            }
        }
    }
}

void
sblinky_spi_irqm_handler(void *arg, int len)
{
    int i;
    struct sblinky_spi_cb_arg *cb;

    //hal_gpio_write(SPI_SS_PIN, 1);

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

        /* copy current tx buf to last */
        memcpy(g_spi_last_tx_buf, g_spi_tx_buf, len);
    } else {
        /* Check that we received what we last sent */
        mmctest_validate_last(len);
    }
    ++g_spi_xfr_num;
}

void
task1_handler(void *arg)
{
    //int i;
    int rc;
    //uint16_t rxval;
    //uint8_t last_val;
    //uint8_t spi_nb_cntr;
    //uint8_t spi_b_cntr;

    struct stm32f4_hal_spi_cfg spi_cfg = {
        .ss_pin   = SPI_SS_PIN,
        .sck_pin  = SPI_SCK_PIN,
        .miso_pin = SPI_MISO_PIN,
        .mosi_pin = SPI_MOSI_PIN,
        .irq_prio = 2
    };

    rc = mmc_init(0, &spi_cfg, spi_cfg.ss_pin);
    printf("mmc_init %d\n", rc);

#if 0
    /*
     * Send some bytes in a non-blocking manner to SPI using tx val. The
     * slave should send back 0x77.
     */
    g_spi_tx_buf[0] = 0xde;
    g_spi_tx_buf[1] = 0xad;
    g_spi_tx_buf[2] = 0xbe;
    g_spi_tx_buf[3] = 0xef;
    hal_gpio_write(SPI_SS_PIN, 0);
    for (i = 0; i < 4; ++i) {
        rxval = hal_spi_tx_val(0, g_spi_tx_buf[i]);
        assert(rxval == 0x77);
        g_spi_rx_buf[i] = (uint8_t)rxval;
    }
    hal_gpio_write(SPI_SS_PIN, 1);
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
        /* Get random length to send */
        g_last_tx_len = spi_cb_obj.txlen;
        spi_cb_obj.txlen = (rand() & 0x1F) + 1;
        memcpy(g_spi_last_tx_buf, g_spi_tx_buf, g_last_tx_len);
        last_val = g_spi_last_tx_buf[g_last_tx_len - 1];
        for (i= 0; i < spi_cb_obj.txlen; ++i) {
            g_spi_tx_buf[i] = (uint8_t)(last_val + i);
        }

        if (g_spi_xfr_num & 1) {
            /* Send non-blocking */
            ++spi_nb_cntr;
            assert(hal_gpio_read(SPI_SS_PIN) == 1);
            hal_gpio_write(SPI_SS_PIN, 0);
#if 0
            if (spi_nb_cntr == 7) {
                g_spi_null_rx = 1;
                rc = hal_spi_txrx_noblock(0, g_spi_tx_buf, NULL, 32);
            } else {
                g_spi_null_rx = 0;
                rc = hal_spi_txrx_noblock(0, g_spi_tx_buf, g_spi_rx_buf, 32);
            }
            assert(!rc);
#else
            g_spi_null_rx = 0;
            rc = hal_spi_txrx_noblock(0, g_spi_tx_buf, g_spi_rx_buf,
                                      spi_cb_obj.txlen);
            assert(!rc);
            console_printf("a transmitted: ");
            for (i = 0; i < spi_cb_obj.txlen; i++) {
                console_printf("%2x ", g_spi_tx_buf[i]);
            }
            console_printf("\n");
            console_printf("received: ");
            for (i = 0; i < spi_cb_obj.txlen; i++) {
                console_printf("%2x ", g_spi_rx_buf[i]);
            }
            console_printf("\n");
#endif
        } else {
            /* Send blocking */
            ++spi_b_cntr;
            assert(hal_gpio_read(SPI_SS_PIN) == 1);
            hal_gpio_write(SPI_SS_PIN, 0);
#if 0
            if (spi_b_cntr == 7) {
                g_spi_null_rx = 1;
                rc = hal_spi_txrx(0, g_spi_tx_buf, NULL, 32);
                spi_b_cntr = 0;
            } else {
                g_spi_null_rx = 0;
                rc = hal_spi_txrx(0, g_spi_tx_buf, g_spi_rx_buf, 32);
            }
            assert(!rc);
            hal_gpio_write(SPI_SS_PIN, 1);
            mmctest_validate_last(spi_cb_obj.txlen);
#else
            rc = hal_spi_txrx(0, g_spi_tx_buf, g_spi_rx_buf, spi_cb_obj.txlen);
            assert(!rc);
            hal_gpio_write(SPI_SS_PIN, 1);
            console_printf("b transmitted: ");
            for (i = 0; i < spi_cb_obj.txlen; i++) {
                console_printf("%2x ", g_spi_tx_buf[i]);
            }
            console_printf("\n");
            console_printf("received: ");
            for (i = 0; i < spi_cb_obj.txlen; i++) {
                console_printf("%2x ", g_spi_rx_buf[i]);
            }
            console_printf("\n");
            mmctest_validate_last(spi_cb_obj.txlen);
            ++g_spi_xfr_num;
#endif
        }
    }
#endif

    while (1) {
        os_time_delay(OS_TICKS_PER_SEC);
    }
}

/**
 * This task serves as a container for the shell and newtmgr packages.  These
 * packages enqueue timer events when they need this task to do work.
 */
static void
task2_handler(void *arg)
{
    while (1) {
        os_eventq_run(&mmctest_evq);
    }
}

static void
task3_handler(void *arg)
{
    while (1) {
        os_time_delay(OS_TICKS_PER_SEC);
        hal_gpio_toggle(g_led_pin);
    }
}

/**
 * init_tasks
 *
 * Called by main.c after sysinit(). This function performs initializations
 * that are required before tasks are running.
 *
 * @return int 0 success; error otherwise.
 */
static void
init_tasks(void)
{
    os_stack_t *pstack;

    /* Initialize global test semaphore */
    os_sem_init(&g_test_sem, 0);

    pstack = malloc(sizeof(os_stack_t)*TASK1_STACK_SIZE);
    assert(pstack);

    os_task_init(&task1, "task1", task1_handler, NULL,
            TASK1_PRIO, OS_WAIT_FOREVER, pstack, TASK1_STACK_SIZE);

    pstack = malloc(sizeof(os_stack_t)*TASK2_STACK_SIZE);
    assert(pstack);

    os_task_init(&task2, "task2", task2_handler, NULL,
            TASK2_PRIO, OS_WAIT_FOREVER, pstack, TASK2_STACK_SIZE);

    pstack = malloc(sizeof(os_stack_t)*TASK3_STACK_SIZE);
    assert(pstack);

    os_task_init(&task3, "task3", task3_handler, NULL,
            TASK3_PRIO, OS_WAIT_FOREVER, pstack, TASK3_STACK_SIZE);

    /* Initialize eventq and designate it as the default.  Packages that need
     * to schedule work items will piggyback on this eventq.  Example packages
     * which do this are sys/shell and mgmt/newtmgr.
     */
    os_eventq_init(&mmctest_evq);
    os_eventq_dflt_set(&mmctest_evq);
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

    /* Set the led pin for the devboard */
    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 0);

    init_tasks();
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return rc;
}

