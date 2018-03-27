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

#ifndef __HAL_APOLLO2_H_
#define __HAL_APOLLO2_H__

#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hal_flash;
extern const struct hal_flash apollo2_flash_dev;

struct apollo2_uart_cfg {
    uint8_t suc_pin_tx;
    uint8_t suc_pin_rx;
    uint8_t suc_pin_rts;
    uint8_t suc_pin_cts;
};

/* SPI configuration (used for both master and slave) */
struct apollo2_spi_cfg {
    uint8_t sck_pin;
    uint8_t mosi_pin;
    uint8_t miso_pin;
    uint8_t ss_pin;
};

#define APOLLO2_TIMER_SOURCE_HFRC       1 /* High-frequency RC oscillator. */
#define APOLLO2_TIMER_SOURCE_XT         2 /* 32.768 kHz crystal oscillator. */
#define APOLLO2_TIMER_SOURCE_LFRC       3 /* Low-frequency RC oscillator. */
#define APOLLO2_TIMER_SOURCE_RTC        4 /* Real time clock. */
#define APOLLO2_TIMER_SOURCE_HCLK       5 /* Main CPU clock. */

struct apollo2_timer_cfg {
    uint8_t source;
};

#ifdef __cplusplus
}
#endif

#endif /* __HAL_APOLLO2_H__ */
