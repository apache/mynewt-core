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
#ifndef __ARDUINO_BSP_H
#define __ARDUINO_BSP_H

#include <mcu/mcu.h>
#include "syscfg/syscfg.h"

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

enum system_device_id
{
    /* NOTE: Some HALs use a virtual enumeration of the devices, while
     * other still use the actual pins (GPIO). For arduino this means
     * that the sysIDs for analog and digital pins are the actual pin
     * numbers */

    ARDUINO_ZERO_D0 =     (11),
    ARDUINO_ZERO_D1 =     (10),
    ARDUINO_ZERO_D3 =     (9),
    ARDUINO_ZERO_D5 =     (15),
    ARDUINO_ZERO_D6 =     (20),
    ARDUINO_ZERO_D7 =     (21),
    ARDUINO_ZERO_D8 =     (6),
    ARDUINO_ZERO_D9 =     (7),
    ARDUINO_ZERO_D10 =    (18),
    ARDUINO_ZERO_D11 =    (16),
    ARDUINO_ZERO_D12 =    (19),
    ARDUINO_ZERO_D13 =    (17),

#if MYNEWT_VAL(BSP_ARDUINO_ZERO_PRO)
    ARDUINO_ZERO_D2 =     (8),
    ARDUINO_ZERO_D4 =     (14),
#else
    ARDUINO_ZERO_D2 =     (14),
    ARDUINO_ZERO_D4 =     (8),
#endif

    /* Use SPI 2 */
    ARDUINO_ZERO_SPI_ICSP = 0,

    /* Use SPI 3 */
    ARDUINO_ZERO_SPI_ALT  = 1,

    /* a I2c port on SCLK and SDA */
    ARDUINO_ZERO_I2C      = 4,
};

#define BSP_WINC1500_SPI_PORT   0

extern uint8_t _ram_start;
#define RAM_SIZE        0x00008000

#define ARDUINO_ZERO_PIN_UART_RX (ARDUINO_ZERO_D0)
#define ARDUINO_ZERO_PIN_UART_TX (ARDUINO_ZERO_D1)

#define LED_BLINK_PIN   (ARDUINO_ZERO_D13)
#define LED_2           (27)

#define CONSOLE_UART_SPEED      115200

/*
 * Wiring of Wifi Shield 101 chip to SAMD21.
 */
#define WINC1500_PIN_RESET      /* PA15 */      MCU_GPIO_PORTA(15)
    /* WINC1500_PIN_WAKE */     /* NC */
#define WINC1500_PIN_IRQ        /* PA21 */      MCU_GPIO_PORTA(21)
    /* WINC1500_PIN_ENABLE */   /* NC */

#define WINC1500_SPI_SPEED                      4000000
#define WINC1500_SPI_SSN        /* PA18 */      MCU_GPIO_PORTA(18)
#define WINC1500_SPI_SCK        /* PB11 */      MCU_GPIO_PORTB(11)
#define WINC1500_SPI_MOSI       /* PB10 */      MCU_GPIO_PORTB(10)
#define WINC1500_SPI_MISO       /* PA12 */      MCU_GPIO_PORTA(12)

#ifdef __cplusplus
}
#endif

#endif  /* __ARDUINO_BSP_H */
