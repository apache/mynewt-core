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

#ifndef H_NRF51_HAL_
#define H_NRF51_HAL_

#ifdef __cplusplus
 extern "C" {
#endif

#include "nrf_hal.h"

struct nrf51_uart_cfg {
    int8_t suc_pin_tx;                          /* pins for IO */
    int8_t suc_pin_rx;
    int8_t suc_pin_rts;
    int8_t suc_pin_cts;
};

const struct nrf51_uart_cfg *bsp_uart_config(void);

struct nrf51_hal_i2c_cfg {
    int scl_pin;
    int sda_pin;
    uint32_t i2c_frequency;
};

/* SPI configuration (used for both master and slave) */
struct nrf51_hal_spi_cfg {
    uint8_t sck_pin;
    uint8_t mosi_pin;
    uint8_t miso_pin;
    uint8_t ss_pin;
};

#ifdef __cplusplus
}
#endif

#endif  /* H_NRF51_HAL_ */

