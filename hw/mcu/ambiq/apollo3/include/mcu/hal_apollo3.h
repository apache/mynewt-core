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

#ifndef __HAL_APOLLO3_H_
#define __HAL_APOLLO3_H__

#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hal_flash;
extern const struct hal_flash apollo3_flash_dev;

struct apollo3_uart_cfg {
    int8_t suc_pin_tx;
    int8_t suc_pin_rx;
    int8_t suc_pin_rts;
    int8_t suc_pin_cts;
};

/* SPI configuration (used for both master and slave) */
struct apollo3_spi_cfg {
    int8_t sck_pin;
    int8_t mosi_pin;
    int8_t miso_pin;
    int8_t ss_pin[4];
};

/* I2C configuration (used for both master and slave) */
struct apollo3_i2c_cfg {
    int8_t scl_pin;
    int8_t sda_pin;
};

#define APOLLO3_TIMER_SOURCE_HFRC       1 /* High-frequency RC oscillator. */
#define APOLLO3_TIMER_SOURCE_XT         2 /* 32.768 kHz crystal oscillator. */
#define APOLLO3_TIMER_SOURCE_LFRC       3 /* Low-frequency RC oscillator. */
#define APOLLO3_TIMER_SOURCE_RTC        4 /* Real time clock. */
#define APOLLO3_TIMER_SOURCE_HCLK       5 /* Main CPU clock. */

struct apollo3_timer_cfg {
    uint8_t source;
};

void apollo3_periph_create(void);

int apollo3_spi_set_ss_pin(int spi_num, int8_t ss_pin);
int apollo3_spi_set_continuation(int spi_num, bool cont);

#ifdef __cplusplus
}
#endif

#endif /* __HAL_APOLLO3_H__ */
