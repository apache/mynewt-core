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

#include <os/mynewt.h>
#include <mcu/stm32_hal.h>
#include <mcu/mcu.h>
#include <bus/drivers/spi_stm32_pins.h>

/*
 * SPI pins of STM32F3 and their alternate functions, from the GPIO data of
 * STM32CubeMX (db/mcu) for lines:
 *   STM32F301, STM32F302, STM32F303, STM32F334, STM32F373, STM32F3x3, STM32F3x8
 * A pin that some line does not have is never configured there.
 */
const struct spi_pin_def spi_stm32_pins[] = {
#if MYNEWT_VAL(SPI_0_MASTER)
    SPI_PIN_DEF(0, MCU_GPIO_PORTA(5), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(0, MCU_GPIO_PORTA(12), SPI_SCK, SPI_AF_6),
    SPI_PIN_DEF(0, MCU_GPIO_PORTB(3), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(0, MCU_GPIO_PORTC(7), SPI_SCK, SPI_AF_5),

    SPI_PIN_DEF(0, MCU_GPIO_PORTA(7), SPI_MOSI, SPI_AF_5),
    SPI_PIN_DEF(0, MCU_GPIO_PORTB(0), SPI_MOSI, SPI_AF_5),
    SPI_PIN_DEF(0, MCU_GPIO_PORTB(5), SPI_MOSI, SPI_AF_5),
    SPI_PIN_DEF(0, MCU_GPIO_PORTC(9), SPI_MOSI, SPI_AF_5),
    SPI_PIN_DEF(0, MCU_GPIO_PORTF(6), SPI_MOSI, SPI_AF_5),

    SPI_PIN_DEF(0, MCU_GPIO_PORTA(6), SPI_MISO, SPI_AF_5),
    SPI_PIN_DEF(0, MCU_GPIO_PORTA(13), SPI_MISO, SPI_AF_6),
    SPI_PIN_DEF(0, MCU_GPIO_PORTB(4), SPI_MISO, SPI_AF_5),
    SPI_PIN_DEF(0, MCU_GPIO_PORTC(8), SPI_MISO, SPI_AF_5),
#endif
#if MYNEWT_VAL(SPI_1_MASTER)
    SPI_PIN_DEF(1, MCU_GPIO_PORTA(8), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTB(8), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTB(10), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTB(13), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTD(7), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTD(8), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTF(1), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTF(9), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTF(10), SPI_SCK, SPI_AF_5),

    SPI_PIN_DEF(1, MCU_GPIO_PORTA(10), SPI_MOSI, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTA(11), SPI_MOSI, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTB(15), SPI_MOSI, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTC(3), SPI_MOSI, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTD(4), SPI_MOSI, SPI_AF_5),

    SPI_PIN_DEF(1, MCU_GPIO_PORTA(9), SPI_MISO, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTA(10), SPI_MISO, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTB(14), SPI_MISO, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTC(2), SPI_MISO, SPI_AF_5),
    SPI_PIN_DEF(1, MCU_GPIO_PORTD(3), SPI_MISO, SPI_AF_5),
#endif
#if MYNEWT_VAL(SPI_2_MASTER)
    SPI_PIN_DEF(2, MCU_GPIO_PORTA(1), SPI_SCK, SPI_AF_6),
    SPI_PIN_DEF(2, MCU_GPIO_PORTB(3), SPI_SCK, SPI_AF_6),
    SPI_PIN_DEF(2, MCU_GPIO_PORTC(10), SPI_SCK, SPI_AF_6),

    SPI_PIN_DEF(2, MCU_GPIO_PORTA(3), SPI_MOSI, SPI_AF_6),
    SPI_PIN_DEF(2, MCU_GPIO_PORTB(5), SPI_MOSI, SPI_AF_6),
    SPI_PIN_DEF(2, MCU_GPIO_PORTC(12), SPI_MOSI, SPI_AF_6),

    SPI_PIN_DEF(2, MCU_GPIO_PORTA(2), SPI_MISO, SPI_AF_6),
    SPI_PIN_DEF(2, MCU_GPIO_PORTB(4), SPI_MISO, SPI_AF_6),
    SPI_PIN_DEF(2, MCU_GPIO_PORTC(11), SPI_MISO, SPI_AF_6),
#endif
#if MYNEWT_VAL(SPI_3_MASTER)
    SPI_PIN_DEF(3, MCU_GPIO_PORTE(2), SPI_SCK, SPI_AF_5),
    SPI_PIN_DEF(3, MCU_GPIO_PORTE(12), SPI_SCK, SPI_AF_5),

    SPI_PIN_DEF(3, MCU_GPIO_PORTE(6), SPI_MOSI, SPI_AF_5),
    SPI_PIN_DEF(3, MCU_GPIO_PORTE(14), SPI_MOSI, SPI_AF_5),

    SPI_PIN_DEF(3, MCU_GPIO_PORTE(5), SPI_MISO, SPI_AF_5),
    SPI_PIN_DEF(3, MCU_GPIO_PORTE(13), SPI_MISO, SPI_AF_5),
#endif
};

const size_t spi_stm32_pin_count = ARRAY_SIZE(spi_stm32_pins);
