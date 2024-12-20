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
#include <stdint.h>
#include <syscfg/syscfg.h>
#include <mcu/nrf54l_hal.h>
#include <bsp/bsp.h>

#if MYNEWT_VAL(UART_0) || MYNEWT_VAL(UART_1) || \
    MYNEWT_VAL(UART_2)
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#endif

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
static const struct nrf54l_uart_cfg os_bsp_uart0_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
};
#endif
#if MYNEWT_VAL(UART_1)
static struct uart_dev os_bsp_uart1;
static const struct nrf54l_uart_cfg os_bsp_uart1_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_1_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_1_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_1_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_1_PIN_CTS),
};
#endif
#if MYNEWT_VAL(UART_2)
static struct uart_dev os_bsp_uart2;
static const struct nrf54l_uart_cfg os_bsp_uart2_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_2_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_2_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_2_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_2_PIN_CTS),
};
#endif

static void
nrf54_periph_create_timers(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(TIMER_0)
    rc = hal_timer_init(0, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_1)
    rc = hal_timer_init(1, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_2)
    rc = hal_timer_init(2, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_3)
    rc = hal_timer_init(3, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_4)
    rc = hal_timer_init(4, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_5)
    rc = hal_timer_init(5, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif
}

static void
nrf54_periph_create_uart(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create(&os_bsp_uart0.ud_dev, "uart0",
                       OS_DEV_INIT_PRIMARY, 0, uart_hal_init,
                       (void *)&os_bsp_uart0_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_1)
    rc = os_dev_create(&os_bsp_uart1.ud_dev, "uart1",
                       OS_DEV_INIT_PRIMARY, 1, uart_hal_init,
                       (void *)&os_bsp_uart1_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_2)
    rc = os_dev_create(&os_bsp_uart2.ud_dev, "uart2",
                       OS_DEV_INIT_PRIMARY, 2, uart_hal_init,
                       (void *)&os_bsp_uart2_cfg);
    assert(rc == 0);
#endif
}

void
nrf54l_periph_create(void)
{
    nrf54_periph_create_timers();
    nrf54_periph_create_uart();
}
