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

#ifndef H_NRF91_HAL_
#define H_NRF91_HAL_

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_hal.h"

struct nrf91_uart_cfg {
    int8_t suc_pin_tx;                          /* pins for IO */
    int8_t suc_pin_rx;
    int8_t suc_pin_rts;
    int8_t suc_pin_cts;
};
const struct nrf91_uart_cfg *bsp_uart_config(void);

struct nrf91_hal_i2c_cfg {
    int scl_pin;
    int sda_pin;
    uint32_t i2c_frequency;
};

extern const struct hal_flash nrf91k_qspi_dev;

/* SPI configuration (used for both master and slave) */
struct nrf91_hal_spi_cfg {
    uint8_t sck_pin;
    uint8_t mosi_pin;
    uint8_t miso_pin;
    uint8_t ss_pin;
};

/*
 * GPIO pin mapping
 *
 * The logical GPIO pin numbers (0 to N) are mapped to ports in the following
 * manner:
 *  pins 0 - 31: Port 0
 *
 *  The nrf9160 has only one port with 32 pins.
 *
 *  NOTE: in order to save code space, there is no checking done to see if the
 *  user specifies a pin that is not used by the processor. If an invalid pin
 *  number is used unexpected and/or erroneous behavior will result.
 */
#ifdef NRF9160_XXAA
#define HAL_GPIO_INDEX(pin)     ((pin) & 0x1F)
#define HAL_GPIO_PORT(pin)      (NRF_P0)
#define HAL_GPIO_MASK(pin)      (1 << HAL_GPIO_INDEX(pin))
#define HAL_GPIOTE_PIN_MASK     (0x3FUL << GPIOTE_CONFIG_PSEL_Pos)
#endif

#ifdef __cplusplus
}
#endif

#endif  /* H_NRF91_HAL_ */

