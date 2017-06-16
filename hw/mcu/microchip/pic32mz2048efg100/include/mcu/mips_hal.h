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

/* This file defines the HAL implementations within this MCU */

#ifndef MIPS_HAL_H
#define MIPS_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* I/O pins for UART */
struct mips_uart_cfg {
    uint8_t tx;
    uint8_t rx;
};

/* I/O pins for SPI, SS pin is not handled by the driver */
struct mips_spi_cfg {
    uint8_t mosi;
    uint8_t miso;
    uint8_t sck;
};

/* I/O pins for I2C, also set frequency */
struct mips_i2c_cfg {
    uint8_t scl;
    uint8_t sda;
    uint32_t frequency;
};

/* Helper functions to enable/disable interrupts. */
#define __HAL_DISABLE_INTERRUPTS(__os_sr) do {__os_sr = __builtin_get_isr_state(); \
        __builtin_disable_interrupts();} while(0)

#define __HAL_ENABLE_INTERRUPTS(__os_sr) __builtin_set_isr_state(__os_sr)

#ifdef __cplusplus
}
#endif

#endif /* MCU_HAL_H */
