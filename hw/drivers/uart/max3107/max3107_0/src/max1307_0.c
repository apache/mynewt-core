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
#include <bsp/bsp.h>
#include <max3107/max3107.h>

static struct max3107_dev max3107_0;

static const struct max3107_cfg max3107_0_cfg = {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    .node_cfg = {
        .node_cfg.lock_timeout_ms = MYNEWT_VAL(MAX3107_LOCK_TIMEOUT),
        .node_cfg.bus_name = MYNEWT_VAL(MAX3107_0_SPI_BUS),
        .freq = MYNEWT_VAL(MAX3107_0_SPI_BAUDRATE),
        .data_order = BUS_SPI_DATA_ORDER_MSB,
        .mode = BUS_SPI_MODE_0,
        .pin_cs = MYNEWT_VAL(MAX3107_0_CS_PIN),
    },
#else
    .spi_settings = {
        .data_order = HAL_SPI_MSB_FIRST,
        .baudrate = MYNEWT_VAL(MAX3107_0_SPI_BAUDRATE),
        .data_mode = HAL_SPI_MODE0,
        .word_size = HAL_SPI_WORD_SIZE_8BIT,
    },
    .spi_num = MYNEWT_VAL(MAX3107_0_SPI_NUM),
    .ss_pin = MYNEWT_VAL(MAX3107_0_CS_PIN),
#endif
    .osc_freq = MYNEWT_VAL(MAX3107_0_OSC_FREQ),
    .irq_pin = MYNEWT_VAL(MAX3107_0_IRQ_PIN),
    .ldoen_pin = MYNEWT_VAL(MAX3107_0_LDOEN_PIN),
    .rx_trigger_level = MYNEWT_VAL(MAX3107_0_UART_RX_FIFO_LEVEL),
    .tx_trigger_level = MYNEWT_VAL(MAX3107_0_UART_TX_FIFO_LEVEL),
    .crystal_en = MYNEWT_VAL(MAX3107_0_CRYSTAL_EN),
    .no_pll = MYNEWT_VAL(MAX3107_0_DISABLE_PLL),
    .allow_mul_4 = 1,
    .allow_mul_2 = 1,
};

static const struct uart_conf_port max3107_0_uart_cfg = {
    .uc_speed = MYNEWT_VAL(MAX3107_0_UART_BAUDRATE),
    .uc_databits = MYNEWT_VAL(MAX3107_0_UART_DATA_BITS),
    .uc_flow_ctl = MYNEWT_VAL(MAX3107_0_UART_FLOW_CONTROL),
    .uc_parity = MYNEWT_VAL(MAX3107_0_UART_PARITY),
    .uc_stopbits = MYNEWT_VAL(MAX3107_0_UART_STOP_BITS),
};

void
max3107_0_init(void)
{
    int rc;

    rc = max3107_dev_create_spi(&max3107_0,
                                MYNEWT_VAL(MAX3107_0_NAME),
                                MYNEWT_VAL(MAX3107_0_UART_NAME),
                                &max3107_0_cfg,
                                &max3107_0_uart_cfg);

    SYSINIT_PANIC_ASSERT(rc == 0);
}
