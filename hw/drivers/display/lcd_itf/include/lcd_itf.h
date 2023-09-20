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

#ifndef LCD_ITF_H
#define LCD_ITF_H

#include <stdint.h>
#include <stddef.h>
#include <syscfg/syscfg.h>

#ifdef LCD_ITF_INCLUDE
#include LCD_ITF_INCLUDE
#endif

#ifndef LCD_CS_PIN_ACTIVE
#if MYNEWT_VAL(LCD_CS_PIN) >= 0
#define LCD_CS_PIN_ACTIVE() hal_gpio_write(MYNEWT_VAL(LCD_CS_PIN), 0)
#else
#define LCD_CS_PIN_ACTIVE()
#endif
#endif

#ifndef LCD_CS_PIN_INACTIVE
#if MYNEWT_VAL(LCD_CS_PIN) >= 0
#define LCD_CS_PIN_INACTIVE() hal_gpio_write(MYNEWT_VAL(LCD_CS_PIN), 1)
#else
#define LCD_CS_PIN_INACTIVE()
#endif
#endif

#ifndef LCD_DC_PIN_DATA
#define LCD_DC_PIN_DATA() hal_gpio_write(MYNEWT_VAL(LCD_DC_PIN), 1)
#endif

#ifndef LCD_DC_PIN_COMMAND
#define LCD_DC_PIN_COMMAND() hal_gpio_write(MYNEWT_VAL(LCD_DC_PIN), 0)
#endif

#ifndef LCD_RESET_PIN_INACTIVE
#if MYNEWT_VAL(LCD_RESET_PIN) >= 0
#define LCD_RESET_PIN_INACTIVE() hal_gpio_write(MYNEWT_VAL(LCD_RESET_PIN), 1)
#else
#define LCD_RESET_PIN_INACTIVE()
#endif
#endif

#ifndef LCD_RESET_PIN_ACTIVE
#if MYNEWT_VAL(LCD_RESET_PIN) >= 0
#define LCD_RESET_PIN_ACTIVE() hal_gpio_write(MYNEWT_VAL(LCD_RESET_PIN), 0)
#else
#define LCD_RESET_PIN_ACTIVE()
#endif
#endif

#define LCD_SEQUENCE_DELAY_REQ                  0xFE
#define LCD_SEQUENCE_DELAY_US_REQ               0xFD
#define LCD_SEQUENCE_LCD_CS_ACTIVATE_REQ        0xFC
#define LCD_SEQUENCE_LCD_CS_INACTIVATE_REQ      0xFB
#define LCD_SEQUENCE_LCD_DC_DATA_REQ            0xFA
#define LCD_SEQUENCE_LCD_DC_COMMAND_REQ         0xF9
#define LCD_SEQUENCE_LCD_RESET_ACTIVATE_REQ     0xF8
#define LCD_SEQUENCE_LCD_RESET_INACTIVATE_REQ   0xF7
#define LCD_SEQUENCE_GPIO_REQ                   0xF6
#define LCD_SEQUENCE(name) \
static const uint8_t name[] = {
#define LCD_SEQUENCE_DELAY(n) LCD_SEQUENCE_DELAY_REQ, (uint8_t)n, ((uint8_t)(n >> 8))
#define LCD_SEQUENCE_DELAY_US(n) LCD_SEQUENCE_DELAY_US_REQ, (uint8_t)n, ((uint8_t)(n >> 8))
#define LCD_SEQUENCE_GPIO(pin, val) LCD_SEQUENCE_GPIO_REQ, n, val
#define LCD_SEQUENCE_LCD_CS_ACTIVATE() LCD_SEQUENCE_LCD_CS_ACTIVATE_REQ
#define LCD_SEQUENCE_LCD_CS_INACTIVATE() LCD_SEQUENCE_LCD_CS_INACTIVATE_REQ
#define LCD_SEQUENCE_LCD_DC_DATA() LCD_SEQUENCE_LCD_DC_DATA_REQ
#define LCD_SEQUENCE_LCD_DC_COMMAND() LCD_SEQUENCE_LCD_DC_COMMAND_REQ
#define LCD_SEQUENCE_LCD_RESET_ACTIVATE() LCD_SEQUENCE_LCD_RESET_ACTIVATE_REQ
#define LCD_SEQUENCE_LCD_RESET_INACTIVATE() LCD_SEQUENCE_LCD_RESET_INACTIVATE_REQ
#define LCD_SEQUENCE_END 0xFF };

void lcd_command_sequence(const uint8_t *cmds);

void lcd_itf_init(void);

/* Function implemented by LCD interface driver */
void lcd_ift_write_cmd(const uint8_t *cmd, int cmd_length);
void lcd_itf_write_color_data(uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2, void *pixels);

#endif /* LCD_ITF_H */
