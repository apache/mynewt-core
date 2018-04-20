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

#ifndef H_BSP_H
#define H_BSP_H

#include <inttypes.h>

#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Define special stackos sections */
#define sec_data_core   __attribute__((section(".data.core")))
#define sec_bss_core    __attribute__((section(".bss.core")))
#define sec_bss_nz_core __attribute__((section(".bss.core.nz")))

/* More convenient section placement macros. */
#define bssnz_t         sec_bss_nz_core

extern uint8_t _ram_start;
#define RAM_SIZE        0x10000

/* LED pins */
#define LED_1           (30)   /* Diode  9   */
#define LED_2           (14)   /* Diode 12   */ 
#define LED_3           (22)   /* Diode 11   */
#define LED_4           (31)   /* Diode 10   */
#define LED_BLINK_PIN   (LED_1)


/* Buttons */
#define BUTTON_1 	(21)   /* RESETn     */
#define BUTTON_2 	(2)    /* BT_WAKE_UP */

    
/* DWM1001 Internal */
#define DWM1001_DW_CS	(17)
#define DWM1001_DW_IRQ	(19)
#define DWM1001_DW_RST	(24)
#define DWM1001_ACC_IRQ	(25)



/* Poorly documented connectors: 

 Right connector (J10):
      Raspberry /   DWM1001      / NRF5|DW1000  |  NRF5|DW1000 /   DWM1001     / Raspberry 
 1 --                                                                            V_RPI   --  2
 3 -- SDA_RPI   / 23 : GPIO_15   / [N ] P0.15   |                                V_RPI   --  4
 5 -- SCL_RPI   / 25 : GPIO_8    / [N ] P0.08   |                                        --  6
 7 --                                           |   [N ] P0.11 / UART_RX  : 18 / TXD_RPI --  8
 9 --                                           |   [N ] P0.05 / UART_TX  : 20 / RXD_RPI -- 10
11 --                                           |   [N ] P0.21 / RESET    : 33           -- 12
13 --                                           |                                        -- 14
15 -- GPIO_RPI  / 19 : READY     / [N ] P0.26   |                                        -- 16
17 --                                           |                                        -- 18
19 -- SPI1_MOSI / 27 : SPIS_MOSI / [N ] P0.06   |                                        -- 20
21 -- SPI1_MISO / 26 : SPIS_MISO / [N ] P0.07   |                                        -- 22
23 -- SPI1_CLK  / 28 : SPIS_CLK  / [N ] P0.04   |   [N ] P0.03 / SPIS_CSn : 29 / CS_RPI  -- 24
25 --                                           |                                        -- 26


 Middle connector (J7):
       DWM1001     / NRF5|DW1000  |  NRF5|DW1000 /      DWM1001    

 1 -- 21 : GPIO_1  / [DW] GPIO1   |  [DW] GPIO0  / GPIO_0  : 22 -- 2
 3 --  6 : GPIO_12 / [N ] P0.12   |  [N ] P0.27  / GPIO_27 : 13 -- 4
 5 -- 14 : I2C_SDA / [N ] P0.29   |  [N ] P0.28  / I2C_SCL : 15 -- 6
 7 -- 16 : GPIO_23 / [N ] P0.23   |  [N ] P0.13  / GPIO_13 : 17 -- 8

*/

    
#ifdef __cplusplus
}
#endif

#endif  /* H_BSP_H */
