/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __ARDUINO_BSP_H
#define __ARDUINO_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

    /* These defines map the stanard arduino pins to virtual IO pins
     * in mynewt
     * MCU */
#define ARDUINO_ZERO_PIN_D0      (11)
#define ARDUINO_ZERO_PIN_D1      (10)
#define ARDUINO_ZERO_PIN_D2      (8)
#define ARDUINO_ZERO_PIN_D3      (9)
#define ARDUINO_ZERO_PIN_D4      (14)
#define ARDUINO_ZERO_PIN_D5      (15)
#define ARDUINO_ZERO_PIN_D6      (20)
#define ARDUINO_ZERO_PIN_D7      (21)  
#define ARDUINO_ZERO_PIN_D8      (6)
#define ARDUINO_ZERO_PIN_D9      (7)
#define ARDUINO_ZERO_PIN_D10     (18)
#define ARDUINO_ZERO_PIN_D11     (16)
#define ARDUINO_ZERO_PIN_D12     (19)
#define ARDUINO_ZERO_PIN_D13     (17)
        
#define ARDUINO_ZERO_PIN_A0      (2)  
#define ARDUINO_ZERO_PIN_A1      (40)
#define ARDUINO_ZERO_PIN_A2      (41)
#define ARDUINO_ZERO_PIN_A3      (4)
#define ARDUINO_ZERO_PIN_A4      (5)
#define ARDUINO_ZERO_PIN_A5      (34)
#define ARDUINO_ZERO_PIN_AREF    (3)
   
#define ARDUINO_ZERO_PIN_I2C_SCL    (23)
#define ARDUINO_ZERO_PIN_I2C_SDA    (22)
    
#define ARDUINO_ZERO_PIN_SPI_SCK    (43)
#define ARDUINO_ZERO_PIN_SPI_MISO    (44)
#define ARDUINO_ZERO_PIN_SPI_MOSI    (42)
    
    /* note this is also connected to ARDUINO_ZERO_PIN_D7.
     * This pin is new in Arduino zero and expected to be a default
     * chip select for the SPI bus on the first shield */
#define ARDUINO_ZERO_PIN_ATN    (21)
    
#define ARDUINO_ZERO_PIN_UART_RX (ARDUINO_ZERO_PIN_D0)    
#define ARDUINO_ZERO_PIN_UART_TX (ARDUINO_ZERO_PIN_D1)    
    
    
#define LED_BLINK_PIN   (ARDUINO_ZERO_PIN_D13)
#define CONSOLE_UART    (2)    
    
int bsp_imgr_current_slot(void);
#ifdef __cplusplus
}
#endif

#endif  /* __ARDUINO_BSP_H */
