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
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_spi.h"
#include "console/console.h"
#include "shell/shell.h"
#include "log/log.h"
#include "stats/stats.h"
#include "config/config.h"
#include <os/os_dev.h>
#include <adc/adc.h>
#include <assert.h>
#include <string.h>
#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif
#include "nrf.h"
#include "app_util_platform.h"
#include "app_error.h"

#ifdef NRF51
#include <adc_nrf51/adc_nrf51.h>
#include "nrf_drv_adc.h"
nrf_drv_adc_config_t adc_config = NRF_DRV_ADC_DEFAULT_CONFIG;
nrf_drv_adc_channel_t g_nrf_adc_chan =
    NRF_DRV_ADC_DEFAULT_CHANNEL(NRF_ADC_CONFIG_INPUT_2);
#endif

#ifdef NRF52
#include <adc_nrf52/adc_nrf52.h>
#include "nrf_drv_saadc.h"
nrf_drv_saadc_config_t adc_config = NRF_DRV_SAADC_DEFAULT_CONFIG;
#endif

#if defined(NRF52) || defined(NRF51)
/* The spi txrx callback */
struct sblinky_spi_cb_arg
{
    int transfers;
    int txlen;
    uint32_t tx_rx_bytes;
};
struct sblinky_spi_cb_arg spi_cb_obj;
void *spi_cb_arg;
#endif

int g_result_mv;
int g_result;

/* Init all tasks */
volatile int tasks_initialized;
int init_tasks(void);

/* Task 1 */
#define TASK1_PRIO (1)
#define TASK1_STACK_SIZE    OS_STACK_ALIGN(1024)
struct os_task task1;
os_stack_t stack1[TASK1_STACK_SIZE];
static volatile int g_task1_loops;

/* Task 2 */
#define TASK2_PRIO (2)
#define TASK2_STACK_SIZE    OS_STACK_ALIGN(1024)
struct os_task task2;
os_stack_t stack2[TASK2_STACK_SIZE];

struct log_handler log_console_handler;
struct log my_log;

static volatile int g_task2_loops;

/* Global test semaphore */
struct os_sem g_test_sem;

/* For LED toggling */
int g_led_pin;

#if MYNEWT_VAL(SPI_MASTER)
uint8_t g_spi_tx_buf[32];
uint8_t g_spi_rx_buf[32];

void
sblinky_spi_irqm_handler(void *arg, int len)
{
    struct sblinky_spi_cb_arg *cb;

    hal_gpio_set(SPI0_CONFIG_CSN_PIN);

    assert(arg == spi_cb_arg);
    if (spi_cb_arg) {
        cb = (struct sblinky_spi_cb_arg *)arg;
        assert(len == cb->txlen);
        ++cb->transfers;
    }
}

#if 1
static void
sblinky_spi_tx_vals(int spi_num, uint8_t *buf, int len)
{
    int i;
    int rc;
    uint8_t *txptr;

    /* Send all bytes in a loop */
    txptr = buf;
    for (i = 0; i < len; ++i) {
        rc = hal_spi_tx_val(0, *txptr);
        assert(rc != 0xFFFF);
        ++txptr;
    }
}
#endif
#endif

#if MYNEWT_VAL(SPI_SLAVE)
uint8_t g_spi_tx_buf[32];
uint8_t g_spi_rx_buf[32];

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
#endif

void
sblinky_spi_cfg(int spi_num)
{
    int spi_id;
    struct hal_spi_settings my_spi;

#if MYNEWT_VAL(SPI_MASTER)
    my_spi.spi_type = HAL_SPI_TYPE_MASTER;
    my_spi.data_order = HAL_SPI_MSB_FIRST;
    my_spi.data_mode = HAL_SPI_MODE0;
    my_spi.baudrate = 8000;
    my_spi.word_size = HAL_SPI_WORD_SIZE_8BIT;
    my_spi.txrx_cb_func = NULL;
    my_spi.txrx_cb_arg = NULL;
    spi_id = 0;
#endif

#if MYNEWT_VAL(SPI_SLAVE)
    my_spi.spi_type = HAL_SPI_TYPE_SLAVE;
    my_spi.data_order = HAL_SPI_MSB_FIRST;
    my_spi.data_mode = HAL_SPI_MODE0;
    my_spi.baudrate = 0;
    my_spi.word_size = HAL_SPI_WORD_SIZE_8BIT;
    my_spi.txrx_cb_func = sblinky_spi_irqs_handler;
    my_spi.txrx_cb_arg = spi_cb_arg;
    spi_id = SPI_SLAVE_ID;
#endif
    hal_spi_config(spi_id, &my_spi);
}

struct adc_dev my_dev;

#if 0
#define SAADC_SAMPLES_IN_BUFFER (4)
static nrf_saadc_value_t       m_buffer_pool[2][SAADC_SAMPLES_IN_BUFFER];

int event_finished;
int total_events;

static void
saadc_cb(const nrf_drv_saadc_evt_t *event)
{
    if (event->type == NRF_DRV_SAADC_EVT_DONE) {
        ++event_finished;
    }
    ++total_events;
}

void
saadc_test(void)
{
    ret_code_t rc;
    nrf_saadc_channel_config_t cc =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN0);

    rc = nrf_drv_saadc_init(NULL, saadc_cb);
    APP_ERROR_CHECK(rc);
    rc = nrf_drv_saadc_channel_init(0, &cc);
    APP_ERROR_CHECK(rc);

    rc = nrf_drv_saadc_buffer_convert(m_buffer_pool[0], SAADC_SAMPLES_IN_BUFFER);
    APP_ERROR_CHECK(rc);

    rc = nrf_drv_saadc_buffer_convert(m_buffer_pool[1], SAADC_SAMPLES_IN_BUFFER);
    APP_ERROR_CHECK(rc);
}
#endif

#define ADC_NUMBER_SAMPLES (2)
#define ADC_NUMBER_CHANNELS (1)

uint8_t *sample_buffer1;
uint8_t *sample_buffer2;

int adc_result;
int my_result_mv;

int
adc_read_event(struct adc_dev *dev, void *arg, uint8_t etype,
        void *buffer, int buffer_len)
{
    int i;
    //int result;
    int rc;

    for (i = 0; i < ADC_NUMBER_SAMPLES; i++) {
        rc = adc_buf_read(dev, buffer, buffer_len, i, &adc_result);
        if (rc != 0) {
            goto err;
        }
        my_result_mv = adc_result_mv(dev, 0, adc_result);
    }

    adc_buf_release(dev, buffer, buffer_len);

    return (0);
err:
    return (rc);
}

void
task1_handler(void *arg)
{
    int rc;
    struct os_task *t;
    struct adc_dev *adc;
#if MYNEWT_VAL(SPI_MASTER)
    int i;
    uint8_t last_val;
#endif

#ifdef NRF52
    nrf_saadc_channel_config_t cc =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN1);
    cc.gain = NRF_SAADC_GAIN1_4;
    cc.reference = NRF_SAADC_REFERENCE_VDD4;
#endif

    /* Set the led pin for the E407 devboard */
    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);

    adc = (struct adc_dev *) os_dev_open("adc0", 1, &adc_config);
    assert(adc != NULL);

#ifdef NRF51
    adc_chan_config(adc, 0, &g_nrf_adc_chan);
#endif
#ifdef NRF52
    adc_chan_config(adc, 0, &cc);
#endif

#if MYNEWT_VAL(SPI_MASTER)
    /* Use SS pin for testing */
    hal_gpio_init_out(SPI0_CONFIG_CSN_PIN, 1);
    sblinky_spi_cfg(0);
    hal_spi_enable(0);

#if 1
    /* Send some bytes in a non-blocking manner to SPI using tx val */
    g_spi_tx_buf[0] = 0xde;
    g_spi_tx_buf[1] = 0xad;
    g_spi_tx_buf[2] = 0xbe;
    g_spi_tx_buf[3] = 0xef;
    sblinky_spi_tx_vals(0, g_spi_tx_buf, 4);

    /* Send blocking with txrx interface */
    for (i = 0; i < 32; ++i) {
        g_spi_tx_buf[i] = i + 1;
    }
    memset(g_spi_rx_buf, 0x55, 32);
    rc = hal_spi_txrx(0, g_spi_tx_buf, g_spi_rx_buf, 32);
    assert(!rc);

    /* Now send with no rx buf and make sure it dont change! */
    for (i = 0; i < 32; ++i) {
        g_spi_tx_buf[i] = (uint8_t)(i + 32);
    }
    memset(g_spi_rx_buf, 0xAA, 32);
    rc = hal_spi_txrx(0, g_spi_tx_buf, NULL, 32);
    assert(!rc);
    for (i = 0; i < 32; ++i) {
        if (g_spi_rx_buf[i] != 0xAA) {
            assert(0);
        }
    }
#endif

    /* Set up for non-blocking */
    hal_spi_disable(0);
    spi_cb_arg = &spi_cb_obj;
    spi_cb_obj.txlen = 32;
    hal_spi_set_txrx_cb(0, sblinky_spi_irqm_handler, spi_cb_arg);
    hal_spi_enable(0);
#endif

#if MYNEWT_VAL(SPI_SLAVE)
    sblinky_spi_cfg(SPI_SLAVE_ID);
    hal_spi_enable(SPI_SLAVE_ID);

    /* Make the default character 0xAA */
    hal_spi_slave_set_def_tx_val(SPI_SLAVE_ID, 0xAA);

    /* Setup a buffer to receive into */
    memset(g_spi_tx_buf, 0xEE, 32);
    rc = hal_spi_txrx(SPI_SLAVE_ID, g_spi_tx_buf, g_spi_rx_buf, 32);
    assert(rc == 0);
#endif

    sample_buffer1 = malloc(adc_buf_size(adc, ADC_NUMBER_CHANNELS, ADC_NUMBER_SAMPLES));
    sample_buffer2 = malloc(adc_buf_size(adc, ADC_NUMBER_CHANNELS, ADC_NUMBER_SAMPLES));
    memset(sample_buffer1, 0, adc_buf_size(adc, ADC_NUMBER_CHANNELS, ADC_NUMBER_SAMPLES));
    memset(sample_buffer2, 0, adc_buf_size(adc, ADC_NUMBER_CHANNELS, ADC_NUMBER_SAMPLES));

#if 1
    adc_buf_set(adc, sample_buffer1, sample_buffer2,
            adc_buf_size(adc, ADC_NUMBER_CHANNELS, ADC_NUMBER_SAMPLES));
    adc_event_handler_set(adc, adc_read_event, (void *) NULL);
#endif

#if 0
    rc = adc_chan_read(adc, 0, &g_result);
    assert(rc == 0);
    g_result_mv = adc_result_mv(adc, 0, g_result);
#endif

    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task1_handler);

        adc_sample(adc);

        ++g_task1_loops;

#if MYNEWT_VAL(SPI_MASTER)
        /*
         * Send a spi buffer using non-blocking callbacks.
         * Every other transfer should use a NULL rxbuf
         */
        last_val = g_spi_tx_buf[31];
        for (i = 0; i < 32; ++i) {
            g_spi_tx_buf[i] = (uint8_t)(last_val + i);
        }
        hal_gpio_clear(SPI0_CONFIG_CSN_PIN);
        rc = hal_spi_txrx(0, g_spi_tx_buf, g_spi_rx_buf, 32);
        assert(!rc);
#endif

        /* Wait one second */
        os_time_delay(OS_TICKS_PER_SEC);

        /* Toggle the LED */
        hal_gpio_toggle(g_led_pin);

#if 0
        nrf_drv_saadc_sample();
#endif
        /* Release semaphore to task 2 */
        os_sem_release(&g_test_sem);
    }

    os_dev_close((struct os_dev *) adc);
}

void
task2_handler(void *arg)
{
#if MYNEWT_VAL(SPI_SLAVE)
    int rc;
#endif
    struct os_task *t;

    while (1) {
        /* just for debug; task 2 should be the running task */
        t = os_sched_get_current_task();
        assert(t->t_func == task2_handler);

        /* Increment # of times we went through task loop */
        ++g_task2_loops;

        /* Wait for semaphore from ISR */
        os_sem_pend(&g_test_sem, OS_TIMEOUT_NEVER);

#if MYNEWT_VAL(SPI_SLAVE)
        /* transmit back what we just received */
        memcpy(g_spi_tx_buf, g_spi_rx_buf, 32);
        rc = hal_spi_txrx(SPI_SLAVE_ID, g_spi_tx_buf, g_spi_rx_buf, 32);
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
    /* Initialize global test semaphore */
    os_sem_init(&g_test_sem, 0);

    os_task_init(&task1, "task1", task1_handler, NULL,
            TASK1_PRIO, OS_WAIT_FOREVER, stack1, TASK1_STACK_SIZE);

    os_task_init(&task2, "task2", task2_handler, NULL,
            TASK2_PRIO, OS_WAIT_FOREVER, stack2, TASK2_STACK_SIZE);

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

    os_init();

    /* Initialize the sblinky log. */
    log_console_handler_init(&log_console_handler);
    log_register("sblinky", &my_log, &log_console_handler);

#if 0
    saadc_test();
#endif
    rc = init_tasks();
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return rc;
}

