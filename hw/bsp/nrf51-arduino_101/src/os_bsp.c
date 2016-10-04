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
#include <assert.h>
#include "syscfg/syscfg.h"
#include "flash_map/flash_map.h"
#include "hal/hal_flash.h"
#include "hal/hal_bsp.h"
#include "bsp/cmsis_nvic.h"
#include "nrf51.h"
#include "nrf51_bitfields.h"
#include "mcu/nrf51_hal.h"
#include "os/os_dev.h"
#include "uart/uart.h"
#include "uart_hal/uart_hal.h"
#include "hal/hal_cputime.h"

#define BSP_LOWEST_PRIO     ((1 << __NVIC_PRIO_BITS) - 1)


#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
static const struct nrf51_uart_cfg os_bsp_uart0_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
};
#endif

void
bsp_init(void)
{
    int rc;

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart0, "uart0",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&os_bsp_uart0_cfg);
    assert(rc == 0);
#endif

    /* Set cputime to count at 1 usec increments */
    rc = cputime_init(MYNEWT_VAL(CLOCK_FREQ));
    assert(rc == 0);
}

extern void timer_handler(void);
static void
rtc0_timer_handler(void)
{
    if (NRF_RTC0->EVENTS_TICK) {
        NRF_RTC0->EVENTS_TICK = 0;
        timer_handler();
    }
}

void
os_bsp_systick_init(uint32_t os_ticks_per_sec, int prio)
{
    uint32_t ctx;
    uint32_t mask;
    uint32_t pre_scaler;

    /* Turn on the LFCLK */
    NRF_CLOCK->XTALFREQ = CLOCK_XTALFREQ_XTALFREQ_16MHz;
    NRF_CLOCK->TASKS_LFCLKSTOP = 1;
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
    NRF_CLOCK->LFCLKSRC = CLOCK_LFCLKSRC_SRC_Xtal;
    NRF_CLOCK->TASKS_LFCLKSTART = 1;

    /* Wait here till started! */
    mask = CLOCK_LFCLKSTAT_STATE_Msk | CLOCK_LFCLKSTAT_SRC_Xtal;
    while (1) {
        if (NRF_CLOCK->EVENTS_LFCLKSTARTED) {
            if ((NRF_CLOCK->LFCLKSTAT & mask) == mask) {
                break;
            }
        }
    }

    /* Is this exact frequency obtainable? */
    pre_scaler = (32768 / os_ticks_per_sec) - 1;

    /* disable interrupts */
    __HAL_DISABLE_INTERRUPTS(ctx);

    NRF_RTC0->TASKS_STOP = 1;
    NRF_RTC0->EVENTS_TICK = 0;
    NRF_RTC0->PRESCALER = pre_scaler;
    NRF_RTC0->INTENCLR = 0xffffffff;
    NRF_RTC0->TASKS_CLEAR = 1;

    /* Set isr in vector table and enable interrupt */
    NVIC_SetPriority(RTC0_IRQn, prio);
    NVIC_SetVector(RTC0_IRQn, (uint32_t)rtc0_timer_handler);
    NVIC_EnableIRQ(RTC0_IRQn);

    NRF_RTC0->INTENSET = RTC_INTENSET_TICK_Msk;
    NRF_RTC0->TASKS_START = 1;

    __HAL_ENABLE_INTERRUPTS(ctx);
}
