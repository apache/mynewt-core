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

#ifndef HW_BUS_DRIVERS_SPI_STM32_PINS_H_
#define HW_BUS_DRIVERS_SPI_STM32_PINS_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SPI functions can only appear on some pins.  To take burden of specifying pin
 * characteristics of the user, following structures and code allow to
 * use simple pin numbers and SPI function to configure pins in MCU.
 */
enum spi_pin_func {
    SPI_SCK,
    SPI_MOSI,
    SPI_MISO
};

enum spi_alt_num {
    SPI_AF_0 = 0,
    SPI_AF_1 = 1,
    SPI_AF_2 = 2,
    SPI_AF_3 = 3,
    SPI_AF_4 = 4,
    SPI_AF_5 = 5,
    SPI_AF_6 = 6,
    SPI_AF_7 = 7,
    SPI_AF_8 = 8,
    SPI_AF_9 = 9,
    SPI_AF_10 = 10,
    SPI_AF_11 = 11,
    SPI_AF_12 = 12,
    SPI_AF_13 = 13,
    SPI_AF_14 = 14,
    SPI_AF_15 = 15,
    SPI_AF_INVALID = 0xFF,
};

struct spi_pin_def {
    /** SPI master number (0-5) */
    uint8_t spi_num;
    /** Pin number should be created using MCU_GPIO_PORTx() macro */
    uint8_t pin_num;
    /** Pin function */
    enum spi_pin_func pin_func;
    /** Alternate function number needed for most STM MCUs (except F1) */
    enum spi_alt_num alt_fun;
};

#define SPI_PIN_DEF(_spi_num, _pin, _func, _alt) { _spi_num, _pin, _func, _alt }

/*
 * Pins of the MCU family, defined in spi_stm32/stm32xxxx/src/spi_pins.c.
 * The first entry matching SPI number, pin and function is used.
 */
extern const struct spi_pin_def spi_stm32_pins[];
extern const size_t spi_stm32_pin_count;

#ifdef __cplusplus
}
#endif

#endif /* HW_BUS_DRIVERS_SPI_STM32_PINS_H_ */
