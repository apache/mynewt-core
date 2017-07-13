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
#ifndef __HIFIVE_BSP_H
#define __HIFIVE_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#define HIFIVE_PIN_0                (16)
#define HIFIVE_PIN_1                (17)
#define HIFIVE_PIN_2                (18)
#define HIFIVE_PIN_3                (19)
#define HIFIVE_PIN_4                (20)
#define HIFIVE_PIN_5                (21)
#define HIFIVE_PIN_6                (22)
#define HIFIVE_PIN_7                (23)
#define HIFIVE_PIN_8                (0)
#define HIFIVE_PIN_9                (1)
#define HIFIVE_PIN_10               (2)
#define HIFIVE_PIN_11               (3)
#define HIFIVE_PIN_12               (4)
#define HIFIVE_PIN_13               (5)

#define HIFIVE_PIN_15               (9)
#define HIFIVE_PIN_16               (10)
#define HIFIVE_PIN_17               (11)
#define HIFIVE_PIN_18               (12)
#define HIFIVE_PIN_19               (13)

#define HIFIVE_UART0_RX             (HIFIVE_PIN_0)
#define HIFIVE_UART0_TX             (HIFIVE_PIN_1)

#define HIFIVE_PWM0_0               (HIFIVE_PIN_8)
#define HIFIVE_PWM0_1               (HIFIVE_PIN_9)
#define HIFIVE_PWM0_2               (HIFIVE_PIN_10)
#define HIFIVE_PWM0_3               (HIFIVE_PIN_11)

#define HIFIVE_PWM1_0               (HIFIVE_PIN_4)
#define HIFIVE_PWM1_1               (HIFIVE_PIN_3)
#define HIFIVE_PWM1_2               (HIFIVE_PIN_5)
#define HIFIVE_PWM1_3               (HIFIVE_PIN_6)

#define HIFIVE_PWM2_0               (HIFIVE_PIN_16)
#define HIFIVE_PWM2_1               (HIFIVE_PIN_17)
#define HIFIVE_PWM2_2               (HIFIVE_PIN_18)
#define HIFIVE_PWM2_3               (HIFIVE_PIN_19)

#define HIFIVE_SPI1_SCK             (HIFIVE_PIN_13)
#define HIFIVE_SPI1_MOSI            (HIFIVE_PIN_11)
#define HIFIVE_SPI1_MISO            (HIFIVE_PIN_12)
#define HIFIVE_SPI1_SS0             (HIFIVE_PIN_10)
#define HIFIVE_SPI1_SS2             (HIFIVE_PIN_15)
#define HIFIVE_SPI1_SS3             (HIFIVE_PIN_16)

/* LED pins */
#define HIFIVE_GREEN_LED_PIN        (HIFIVE_PIN_3)
#define HIFIVE_BLUE_LED_PIN         (HIFIVE_PIN_5)
#define HIFIVE_RED_LED_PIN          (HIFIVE_PIN_6)

#define LED_BLINK_PIN               (HIFIVE_BLUE_LED_PIN)

#define CONSOLE_UART                "uart0"

extern uint8_t _ram_start;
#define RAM_SIZE        0x8000

#ifdef __cplusplus
}
#endif

#endif  /* __HIFIVE_BSP_H */
