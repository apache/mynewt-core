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

#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "os/mynewt.h"
#include <hal/hal_gpio.h>
#include <hal/hal_uart.h>
#include <hal/hal_timer.h>

#include <uart/uart.h>

#include "uart_bitbang/uart_bitbang.h"

/*
 * Async UART as a bitbanger.
 * Cannot run very fast, as it relies on cputimer to time sampling and
 * bit tx start times.
 */
struct uart_bitbang {
    int ub_bittime;             /* number of cputimer ticks per bit */
    struct {
        int pin;                /* RX pin */
        struct hal_timer timer;
        uint32_t start;         /* cputime when byte rx started */
        uint8_t byte;           /* receiving this byte */
        uint8_t bits;           /* how many bits we've seen */
        int false_irq;
    } ub_rx;
    struct {
        int pin;                /* TX pin */
        struct hal_timer timer;
        uint32_t start;         /* cputime when byte tx started */
        uint8_t byte;           /* byte being transmitted */
        uint8_t bits;           /* how many bits have been sent */
    } ub_tx;

    uint8_t ub_open:1;
    uint8_t ub_rx_stall:1;
    uint8_t ub_txing:1;
    uint32_t ub_cputimer_freq;
    hal_uart_rx_char ub_rx_func;
    hal_uart_tx_char ub_tx_func;
    hal_uart_tx_done ub_tx_done;
    void *ub_func_arg;
};

/*
 * Bytes start with START bit (0) followed by 8 data bits and then the
 * STOP bit (1). STOP bit should be configurable. Data bits are sent LSB first.
 */
static void
uart_bitbang_tx_timer(void *arg)
{
    struct uart_bitbang *ub = (struct uart_bitbang *)arg;
    uint32_t next = 0;
    int data;

    if (!ub->ub_txing || ub->ub_tx.bits > 9) {
        if (ub->ub_tx.bits > 9) {
            if (ub->ub_tx_done) {
                ub->ub_tx_done(ub->ub_func_arg);
            }
        }
        data = ub->ub_tx_func(ub->ub_func_arg);
        if (data < 0) {
            ub->ub_txing = 0;
            return;
        } else {
            ub->ub_tx.byte = data;
        }
        /*
         * Start bit
         */
        hal_gpio_write(ub->ub_tx.pin, 0);
        ub->ub_tx.start = os_cputime_get32();
        next = ub->ub_tx.start + ub->ub_bittime;
        ub->ub_txing = 1;
        ub->ub_tx.bits = 0;
    } else {
        if (ub->ub_tx.bits++ < 8) {
            hal_gpio_write(ub->ub_tx.pin, ub->ub_tx.byte & 0x01);
            ub->ub_tx.byte = ub->ub_tx.byte >> 1;
            next = ub->ub_tx.start + (ub->ub_bittime * (ub->ub_tx.bits + 1));
        } else {
            /*
             * STOP bit.
             */
            hal_gpio_write(ub->ub_tx.pin, 1);
            next = ub->ub_tx.start + (ub->ub_bittime * 10);
        }
    }
    os_cputime_timer_start(&ub->ub_tx.timer, next);
}

static void
uart_bitbang_rx_timer(void *arg)
{
    struct uart_bitbang *ub = (struct uart_bitbang *)arg;
    int val;

    val = hal_gpio_read(ub->ub_rx.pin);

    if (val) {
        ub->ub_rx.byte = 0x80 | (ub->ub_rx.byte >> 1);
    } else {
        ub->ub_rx.byte = (ub->ub_rx.byte >> 1);
    }
    if (ub->ub_rx.bits == 7) {
        val = ub->ub_rx_func(ub->ub_func_arg, ub->ub_rx.byte);
        if (val) {
            ub->ub_rx_stall = 1;
        } else {
            /*
             * Re-enable GPIO IRQ after we've sampled last bit. STOP bit
             * is ignored.
             */
            hal_gpio_irq_enable(ub->ub_rx.pin);
        }
    } else {
        ub->ub_rx.bits++;
        os_cputime_timer_start(&ub->ub_rx.timer,
          ub->ub_rx.start + (ub->ub_bittime * (ub->ub_rx.bits + 1)) +
          (ub->ub_bittime >> 1));
    }
}

/*
 * Byte RX starts when we get transition from high to low.
 * We disable RX irq after we've seen start until end of RX of one byte.
 */
static void
uart_bitbang_isr(void *arg)
{
    struct uart_bitbang *ub = (struct uart_bitbang *)arg;
    uint32_t time;

    time = os_cputime_get32();
    if (ub->ub_rx.start - time < (9 * ub->ub_bittime)) {
        ++ub->ub_rx.false_irq;
        return;
    }
    ub->ub_rx.start = time;
    ub->ub_rx.byte = 0;
    ub->ub_rx.bits = 0;

    /*
     * We try to sample in the middle of a bit. First sample is taken
     * 1.5 bittimes after beginning of start bit.
     */
    os_cputime_timer_start(&ub->ub_rx.timer, time +
        ub->ub_bittime + (ub->ub_bittime >> 1));

    hal_gpio_irq_disable(ub->ub_rx.pin);
}

static void
uart_bitbang_blocking_tx(struct uart_dev *dev, uint8_t data)
{
    struct uart_bitbang *ub;
    int i;
    uint32_t start, next;

    ub = (struct uart_bitbang *)dev->ud_priv;
    if (!ub->ub_open) {
        return;
    }
    hal_gpio_write(ub->ub_tx.pin, 0);
    start = os_cputime_get32();
    next = start + ub->ub_bittime;
    while (os_cputime_get32() < next);
    for (i = 0; i < 8; i++) {
        hal_gpio_write(ub->ub_tx.pin, data & 0x01);
        data = data >> 1;
        next = start + (ub->ub_bittime * i + 1);
        while (os_cputime_get32() < next);
    }
    next = start + (ub->ub_bittime * 10);
    hal_gpio_write(ub->ub_tx.pin, 1);
    while (os_cputime_get32() < next);
}

static void
uart_bitbang_start_tx(struct uart_dev *dev)
{
    struct uart_bitbang *ub;
    int sr;

    ub = (struct uart_bitbang *)dev->ud_priv;
    if (!ub->ub_open) {
        return;
    }
    if (!ub->ub_txing) {
        OS_ENTER_CRITICAL(sr);
        uart_bitbang_tx_timer(ub);
        OS_EXIT_CRITICAL(sr);
    }
}

static void
uart_bitbang_start_rx(struct uart_dev *dev)
{
    struct uart_bitbang *ub;
    int sr;
    int rc;

    ub = (struct uart_bitbang *)dev->ud_priv;
    if (ub->ub_rx_stall) {
        rc = ub->ub_rx_func(ub->ub_func_arg, ub->ub_rx.byte);
        if (rc == 0) {
            OS_ENTER_CRITICAL(sr);
            ub->ub_rx_stall = 0;
            OS_EXIT_CRITICAL(sr);

            /*
             * Start looking for start bit again.
             */
            hal_gpio_irq_enable(ub->ub_rx.pin);
        }
    }
}

static int
uart_bitbang_config(struct uart_bitbang *ub, int32_t baudrate, uint8_t databits,
  uint8_t stopbits, enum hal_uart_parity parity,
  enum hal_uart_flow_ctl flow_ctl)
{
    if (databits != 8 || parity != HAL_UART_PARITY_NONE ||
      flow_ctl != HAL_UART_FLOW_CTL_NONE) {
        return -1;
    }

    assert(ub->ub_rx.pin != ub->ub_tx.pin); /* make sure it's initialized */

    if (baudrate > 19200) {
        return -1;
    }
    ub->ub_bittime = ub->ub_cputimer_freq / baudrate;

    os_cputime_timer_init(&ub->ub_rx.timer, uart_bitbang_rx_timer, ub);
    os_cputime_timer_init(&ub->ub_tx.timer, uart_bitbang_tx_timer, ub);

    if (hal_gpio_init_out(ub->ub_tx.pin, 1)) {
        return -1;
    }

    if (hal_gpio_irq_init(ub->ub_rx.pin, uart_bitbang_isr, ub,
        HAL_GPIO_TRIG_FALLING, HAL_GPIO_PULL_UP)) {
        return -1;
    }
    hal_gpio_irq_enable(ub->ub_rx.pin);

    ub->ub_open = 1;
    return 0;
}

static int
uart_bitbang_open(struct os_dev *odev, uint32_t wait, void *arg)
{
    struct uart_dev *dev = (struct uart_dev *)odev;
    struct uart_bitbang *ub;
    struct uart_conf *uc;

    ub = (struct uart_bitbang *)dev->ud_priv;
    uc = (struct uart_conf *)arg;

    ub->ub_rx_func = uc->uc_rx_char;
    ub->ub_tx_func = uc->uc_tx_char;
    ub->ub_tx_done = uc->uc_tx_done;
    ub->ub_func_arg = uc->uc_cb_arg;

    if (uart_bitbang_config(ub, uc->uc_speed, uc->uc_databits,
        uc->uc_stopbits, uc->uc_parity, uc->uc_flow_ctl)) {
        return OS_EINVAL;
    }
    return OS_OK;
}

static int
uart_bitbang_close(struct os_dev *odev)
{
    struct uart_dev *dev = (struct uart_dev *)odev;
    struct uart_bitbang *ub;
    int sr;

    ub = (struct uart_bitbang *)dev->ud_priv;
    OS_ENTER_CRITICAL(sr);
    hal_gpio_irq_disable(ub->ub_rx.pin);
    hal_gpio_irq_release(ub->ub_rx.pin);
    ub->ub_open = 0;
    ub->ub_txing = 0;
    ub->ub_rx_stall = 0;
    os_cputime_timer_stop(&ub->ub_tx.timer);
    os_cputime_timer_stop(&ub->ub_rx.timer);
    OS_EXIT_CRITICAL(sr);
    return OS_OK;
}

int
uart_bitbang_init(struct os_dev *odev, void *arg)
{
    struct uart_dev *dev = (struct uart_dev *)odev;
    struct uart_bitbang *ub;
    struct uart_bitbang_conf *ubc;

    ub = (struct uart_bitbang *)os_malloc(sizeof(struct uart_bitbang));
    if (!ub) {
        return OS_ENOMEM;
    }
    memset(ub, 0, sizeof(*ub));

    ubc = (struct uart_bitbang_conf *)arg;
    ub->ub_rx.pin = ubc->ubc_rxpin;
    ub->ub_tx.pin = ubc->ubc_txpin;
    ub->ub_cputimer_freq = ubc->ubc_cputimer_freq;

    OS_DEV_SETHANDLERS(odev, uart_bitbang_open, uart_bitbang_close);

    dev->ud_funcs.uf_start_tx = uart_bitbang_start_tx;
    dev->ud_funcs.uf_start_rx = uart_bitbang_start_rx;
    dev->ud_funcs.uf_blocking_tx = uart_bitbang_blocking_tx;
    dev->ud_priv = ub;

    return OS_OK;
}

