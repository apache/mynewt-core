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

#ifndef H_BSP_
#define H_BSP_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Define special stackos sections */
#define sec_data_core   __attribute__((section(".data.core")))
#define sec_bss_core    __attribute__((section(".bss.core")))
#define sec_bss_nz_core __attribute__((section(".bss.core.nz")))

/* More convenient section placement macros. */
#define bssnz_t         sec_bss_nz_core

/** Defined in MCU linker script. */
extern uint8_t _ram_start;

#define RAM_SIZE        0x10000

/* Battery charger SMG40561 */
#define CHARGER_POWER_PRESENCE_PIN  19 /* P0.19 POWER PRESENCE INDICATION IN */
#define CHARGER_CHARGE_PIN      12 /* P0.12 CHARGE INDICATION         IN */
#define BATTERY_VOLTAGE_PIN     31 /* P0.31 BATTERYVOLTAGE (Analog)   IN */

/* Periphiral power control */
#define POWER_CONTROL_PIN       24 /* P0.24 3V3POWERCONTROL      OUT */

/* 2 wire power button */
#define PUSH_BUTTON_IN_PIN      13 /* P0.13 PUSH BUTTON_IN       IN */
#define PUSH_BUTTON_OUT_PIN     15 /* P0.15 PUSH BUTTON_OUT      OUT */

/* Vibrator motor */
#define VIBRATOR_PIN            16 /* P0.16 VIBRATOR             OUT */

/* LCD display */
#define LCD_SPI_CLOCK_PIN        2 /* P0.02 SPI-SCK, LCD_SCK     OUT */
#define LCD_SPI_MOSI_PIN         3 /* P0.03 SPI-MOSI, LCD_SDI    OUT */
#define LCD_DET_PIN              9 /* P0.09 LCD_DET              OUT */
#define LCD_WRITE_PIN           18 /* P0.18 LCD_RS               OUT */
#define LCD_BACKLIGHT_LOW_PIN   14 /* P0.14 LCD_BACKLIGHT_LOW    OUT */
#define LCD_BACKLIGHT_MED_PIN   22 /* P0.22 LCD_BACKLIGHT_MID    OUT */
#define LCD_BACKLIGHT_HIGH_PIN  23 /* P0.23 LCD_BACKLIGHT_HIGH   OUT */
#define LCD_CHIP_SELECT_PIN     25 /* P0.25 LCD_CS               OUT */
#define LCD_RESET_PIN           26 /* P0.26 LCD_RESET            OUT */

/* SPI NOR Flash PM25LV512 */
#define FLASH_CLOCK_PIN          2 /* P0.02 SPI-SCK, LCD_SCK     OUT */
#define FLASH_MOSI_PIN           3 /* P0.03 SPI-MOSI, LCD_SDI    OUT */
#define FLASH_MISO_PIN           4 /* P0.04 SPI-MISO             IN */
#define FLASH_SELECT_PIN         5 /* P0.05 SPI-CE#(SPI-NOR)     OUT */

/* Accelerometer i2c BMA421 */
#define ACCELEROMETER_DATA_PIN   6 /* P0.06 BMA421-SDA, HRS3300-SDA, TP-SDA  I/O */
#define ACCELEROMETER_CLOCK_PIN  7 /* P0.07 BMA421-SCL, HRS3300-SCL, TP-SCL  OUT */
#define ACCELEROMETER_INT_PIN    8 /* P0.08 BMA421-INT                       IN */

/* Touchscreen controller i2c */
#define TOUCH_DATA_PIN           6 /* P0.06 BMA421-SDA, HRS3300-SDA, TP-SDA  I/O */
#define TOUCH_CLOCK_PIN          7 /* P0.07 BMA421-SCL, HRS3300-SCL, TP-SCL  OUT */
#define TOUCH_INT_PIN           28 /* P0.28 TP_INT                           IN */
#define TOUCH_RESET_PIN         10 /* P0.10 TP_RESET                         OUT */

/* Heartrate sensor i2c */
#define HEARTRATE_DATA_PIN       6 /* P0.06 BMA421-SDA, HRS3300-SDA, TP-SDA  I/O */
#define HEARTRATE_CLOCK_PIN      7 /* P0.07 BMA421-SCL, HRS3300-SCL, TP-SCL  OUT */
#define HEARTRATE_INT_PIN       30 /* P0.30 HRS3300-TEST                     IN */

/* apps/blinky support */
#define LED_BLINK_PIN   LCD_BACKLIGHT_HIGH_PIN


#ifdef __cplusplus
}
#endif

#endif
