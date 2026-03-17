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

#include <assert.h>
#include <string.h>
#include <syscfg/syscfg.h>
#include <sysinit/sysinit.h>
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <class/cdc/cdc_device.h>
#include <cdc/cdc.h>
#include <uart/uart.h>

#define TX_LED_PIN      MYNEWT_VAL(CDC_UART_TX_LED_PIN)
#define RX_LED_PIN      MYNEWT_VAL(CDC_UART_RX_LED_PIN)
#define TX_LED_ON_VALUE MYNEWT_VAL(CDC_UART_TX_LED_ON_VALUE)
#define RX_LED_ON_VALUE MYNEWT_VAL(CDC_UART_RX_LED_ON_VALUE)

static struct os_event tx_flush_event;
static const struct cdc_callbacks cdc_cdc_uart_callback;
static struct uart_dev *uart_dev;
static bool tx_led_state;
static bool rx_led_state;

static int cdc_uart_tx_char(void *arg);
static int cdc_uart_rx_char(void *arg, uint8_t b);
static void cdc_uart_tx_done(void *arg);

struct uart_conf cdc_uart_conf = {
    .uc_speed = 1000000,
    .uc_databits = 8,
    .uc_stopbits = 1,
    .uc_parity = UART_PARITY_NONE,
    .uc_flow_ctl = UART_FLOW_CTL_NONE,
    .uc_tx_char = cdc_uart_tx_char,
    .uc_rx_char = cdc_uart_rx_char,
    .uc_tx_done = cdc_uart_tx_done,
    .uc_cb_arg = NULL,
};

typedef struct fifo {
    uint8_t *buf;
    int uart_rx_wr;
    int uart_rx_rd;
    int buf_size;
    int mask;
} fifo_t;

static uint8_t uart_rx_fifo_buf[MYNEWT_VAL(CDC_UART_RX_FIFO_SIZE)];
fifo_t uart_rx_fifo = {
    .buf = uart_rx_fifo_buf,
    .buf_size = sizeof(uart_rx_fifo_buf),
    .mask = (sizeof(uart_rx_fifo_buf) * 2) - 1,
};

static uint8_t uart_tx_fifo_buf[MYNEWT_VAL(CDC_UART_TX_FIFO_SIZE)];
fifo_t uart_tx_fifo = {
    .buf = uart_tx_fifo_buf,
    .buf_size = sizeof(uart_tx_fifo_buf),
    .mask = (sizeof(uart_tx_fifo_buf) * 2) - 1,
};

int
fifo_put(fifo_t *fifo, uint8_t b)
{
    int rd = fifo->uart_rx_rd;
    int wr = fifo->uart_rx_wr;
    int mask2 = fifo->mask;
    int mask1 = mask2 >> 1;
    if (((wr - rd) & mask2) < fifo->buf_size) {
        fifo->buf[wr & mask1] = b;
        fifo->uart_rx_wr = (wr + 1) & fifo->mask;
        return 1;
    } else {
        return -1;
    }
}

int
fifo_get(fifo_t *fifo)
{
    int ret = -1;
    int rd = fifo->uart_rx_rd;
    int wr = fifo->uart_rx_wr;
    int mask2 = fifo->mask;
    int mask1 = mask2 >> 1;
    if (rd != wr) {
        ret = fifo->buf[rd & mask1];
        fifo->uart_rx_rd = (rd + 1) & mask2;
    }
    return ret;
}

int
fifo_available(const fifo_t *fifo)
{
    int rd = fifo->uart_rx_rd;
    int wr = fifo->uart_rx_wr;
    int mask2 = fifo->mask;
    int mask1 = mask2 >> 1;
    return (wr - rd) & mask1;
}

int
fifo_space(const fifo_t *fifo)
{
    return fifo->buf_size - fifo_available(fifo);
}

typedef struct cdc_cdc_uart_itf {
    /* CDC Interface */
    cdc_itf_t cdc_itf;
    bool uart_tx_in_progress;
} cdc_cdc_uart_itf_t;

static cdc_cdc_uart_itf_t cdc_cdc_uart_itf = {
    .cdc_itf.callbacks = &cdc_cdc_uart_callback,
};

static void
tx_led_on(void)
{
    tx_led_state = true;
    if (TX_LED_PIN >= 0) {
        if (TX_LED_PIN != RX_LED_PIN) {
            hal_gpio_write(TX_LED_PIN, TX_LED_ON_VALUE);
        } else {
            hal_gpio_write(TX_LED_PIN, TX_LED_ON_VALUE);
        }
    }
}

static void
tx_led_off(void)
{
    tx_led_state = false;
    if (TX_LED_PIN >= 0) {
        if (TX_LED_PIN != RX_LED_PIN) {
            hal_gpio_write(TX_LED_PIN, !TX_LED_ON_VALUE);
        } else {
            if (!rx_led_state) {
                hal_gpio_write(TX_LED_PIN, !TX_LED_ON_VALUE);
            }
        }
    }
}

static void
rx_led_on(void)
{
    rx_led_state = true;
    if (RX_LED_PIN >= 0) {
        if (TX_LED_PIN != RX_LED_PIN) {
            hal_gpio_write(RX_LED_PIN, RX_LED_ON_VALUE);
        } else {
            hal_gpio_write(RX_LED_PIN, RX_LED_ON_VALUE);
        }
    }
}

static void
rx_led_off(void)
{
    rx_led_state = false;
    if (RX_LED_PIN >= 0) {
        if (TX_LED_PIN != RX_LED_PIN) {
            hal_gpio_write(RX_LED_PIN, !RX_LED_ON_VALUE);
        } else {
            if (!tx_led_state) {
                hal_gpio_write(RX_LED_PIN, !RX_LED_ON_VALUE);
            }
        }
    }
}

static void
cdc_cdc_uart_schedule_tx_flush(void)
{
    os_eventq_put(os_eventq_dflt_get(), &tx_flush_event);
}

static void
tx_flush_ev_cb(struct os_event *ev)
{
    int n;

    while (fifo_available(&uart_rx_fifo)) {
        n = tud_cdc_n_write_char(cdc_cdc_uart_itf.cdc_itf.cdc_num,
                                 (char)fifo_get(&uart_rx_fifo));
        if (n == 0) {
            break;
        }
    }
    uart_start_rx(uart_dev);
    tud_cdc_n_write_flush(cdc_cdc_uart_itf.cdc_itf.cdc_num);
}

static int
cdc_uart_tx_char(void *arg)
{
    int ret = -1;

    tx_led_on();
    if (fifo_available(&uart_tx_fifo)) {
        ret = fifo_get(&uart_tx_fifo);
    }
    return ret;
}

static int
cdc_uart_rx_char(void *arg, uint8_t b)
{
    int ret = fifo_put(&uart_rx_fifo, b);

    rx_led_on();
    cdc_cdc_uart_schedule_tx_flush();

    return ret;
}

static void
cdc_uart_tx_done(void *arg)
{
    cdc_cdc_uart_itf.uart_tx_in_progress = false;
    tx_led_off();
}

static void
cdc_cdc_uart_rx_cb(cdc_itf_t *itf)
{
    cdc_cdc_uart_itf_t *cdc_itf = (cdc_cdc_uart_itf_t *)itf;
    int b;

    while (tud_cdc_n_available(itf->cdc_num) && fifo_space(&uart_tx_fifo)) {
        b = tud_cdc_n_read_char(itf->cdc_num);
        fifo_put(&uart_tx_fifo, b);
    }
    if (!cdc_itf->uart_tx_in_progress) {
        cdc_itf->uart_tx_in_progress = true;
        uart_start_tx(uart_dev);
    }
}

static void
cdc_cdc_uart_line_state_cb(cdc_itf_t *itf, bool dtr, bool rts)
{
}

static uint8_t _cdc_stop_bits[] = { 1, 1, 2, 1, 1 };
static uint8_t
stop_bits(uint8_t cdc_stop_bits)
{
    return _cdc_stop_bits[cdc_stop_bits];
}

static void
cdc_cdc_uart_coding_cb(cdc_itf_t *itf, cdc_line_coding_t const *p_line_coding)
{
    (void)itf;

    if (uart_dev) {
        if (cdc_uart_conf.uc_speed == p_line_coding->bit_rate &&
            cdc_uart_conf.uc_databits == stop_bits(p_line_coding->data_bits) &&
            cdc_uart_conf.uc_parity == p_line_coding->parity) {
            return;
        }
        os_dev_close(&uart_dev->ud_dev);
    }
    cdc_uart_conf.uc_parity = p_line_coding->parity;
    cdc_uart_conf.uc_databits = p_line_coding->data_bits;
    cdc_uart_conf.uc_speed = p_line_coding->bit_rate;
    cdc_uart_conf.uc_stopbits = stop_bits(p_line_coding->stop_bits);
    uart_dev = uart_open(MYNEWT_VAL(CDC_UART_UART_DEV_NAME), &cdc_uart_conf);
}

static void
cdc_cdc_uart_send_break_cb(cdc_itf_t *itf, uint16_t duration_ms)
{
}

static void
cdc_cdc_uart_tx_complete_cb(cdc_itf_t *itf)
{
    rx_led_off();
    cdc_cdc_uart_schedule_tx_flush();
}

static const struct cdc_callbacks cdc_cdc_uart_callback = {
    .cdc_rx_cb = cdc_cdc_uart_rx_cb,
    .cdc_line_coding_cb = cdc_cdc_uart_coding_cb,
    .cdc_line_state_cb = cdc_cdc_uart_line_state_cb,
    .cdc_rx_wanted_cb = NULL,
    .cdc_send_break_cb = cdc_cdc_uart_send_break_cb,
    .cdc_tx_complete_cb = cdc_cdc_uart_tx_complete_cb,
};

void
cdc_uart_init(void)
{
    SYSINIT_ASSERT_ACTIVE();

    if (TX_LED_PIN >= 0) {
        hal_gpio_init_out(TX_LED_PIN, !TX_LED_ON_VALUE);
    }
    if (RX_LED_PIN >= 0 && RX_LED_PIN != TX_LED_PIN) {
        hal_gpio_init_out(RX_LED_PIN, !RX_LED_ON_VALUE);
    }
    tx_flush_event.ev_cb = tx_flush_ev_cb;
    uart_dev = uart_open(MYNEWT_VAL(CDC_UART_UART_DEV_NAME), &cdc_uart_conf);

    cdc_itf_add(&cdc_cdc_uart_itf.cdc_itf);
}
