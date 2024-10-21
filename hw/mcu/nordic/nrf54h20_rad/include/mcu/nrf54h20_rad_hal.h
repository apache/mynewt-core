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

#ifndef _NRF5340_NET_HAL_H_
#define _NRF5340_NET_HAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <hal/hal_flash_int.h>
#include "nrf_hal.h"

struct nrf54h20_rad_uart_cfg {
    int8_t suc_pin_tx;                          /* pins for IO */
    int8_t suc_pin_rx;
    int8_t suc_pin_rts;
    int8_t suc_pin_cts;
};
const struct nrf54h20_rad_uart_cfg *bsp_uart_config(void);

struct nrf54h20_vflash {
    struct hal_flash nv_flash;
    const uint8_t *nv_image_address;
    uint32_t nv_image_size;
    const struct flash_area *nv_slot1;
};
extern struct nrf54h20_vflash nrf54h20_rad_vflash_dev;

extern const struct hal_flash nrf54h20_flash_dev;
extern const struct hal_flash *ipc_flash(void);

/* SPI configuration (used for both master and slave) */
struct nrf54h20_rad_hal_spi_cfg {
    int8_t sck_pin;
    int8_t mosi_pin;
    int8_t miso_pin;
    int8_t ss_pin;
};

/*
 * GPIO pin mapping
 *
 * The logical GPIO pin numbers (0 to N) are mapped to ports in the following
 * manner:
 *  pins 0 - 31: Port 0
 *  pins 32 - 48: Port 1.
 *
 *  The nrf54h20 has 48 pins and uses two ports.
 *
 *  NOTE: in order to save code space, there is no checking done to see if the
 *  user specifies a pin that is not used by the processor. If an invalid pin
 *  number is used unexpected and/or erroneous behavior will result.
 */

#define HAL_GPIO_INDEX(pin)     ((pin) & 0x1F)
#define HAL_GPIO_PORT(pin)      ((pin) > 31 ? NRF_P1_NS : NRF_P0_NS)
#define HAL_GPIO_MASK(pin)      (1 << HAL_GPIO_INDEX(pin))
#define HAL_GPIOTE_PIN_MASK     (0x3FUL << GPIOTE_CONFIG_PSEL_Pos)

#ifdef __cplusplus
}
#endif

#endif  /* _NRF5340_NET_HAL_H_ */

