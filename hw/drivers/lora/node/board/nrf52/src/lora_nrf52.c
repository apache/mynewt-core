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
#include "syscfg/syscfg.h"
#include "hal/hal_gpio.h"
#include "hal/hal_spi.h"
#include "board/board.h"

void
lora_node_nrf52_init(void)
{
    struct hal_spi_settings spi_settings;
    int rc;

    hal_gpio_init_out(RADIO_NSS, 1);

    spi_settings.data_order = HAL_SPI_MSB_FIRST;
    spi_settings.data_mode = HAL_SPI_MODE0;
    spi_settings.baudrate = MYNEWT_VAL(LORA_NODE_BOARD_SPI_BAUDRATE);
    spi_settings.word_size = HAL_SPI_WORD_SIZE_8BIT;

    rc = hal_spi_config(RADIO_SPI_IDX, &spi_settings);
    SYSINIT_PANIC_ASSERT_MSG(rc == 0, "Failed to configure LoRa SPI");

    rc = hal_spi_enable(0);
    SYSINIT_PANIC_ASSERT_MSG(rc == 0, "Failed to enable LoRa SPI");
}
