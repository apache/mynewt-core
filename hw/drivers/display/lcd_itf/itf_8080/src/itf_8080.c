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

#include <bsp/bsp.h>
#include <hal/hal_gpio.h>

#include <lv_conf.h>
#include <lcd_itf.h>

#define PIN(b, pin) ((b >> pin) & 1)

#ifndef LCD_ITF_8080_WRITE_BYTE
#define LCD_ITF_8080_WRITE_BYTE(n) lcd_itf_8080_write_byte(n)

void
lcd_itf_8080_write_byte(uint8_t b)
{
    hal_gpio_write(MYNEWT_VAL(LCD_D0_PIN), PIN(b, 0));
    hal_gpio_write(MYNEWT_VAL(LCD_D1_PIN), PIN(b, 1));
    hal_gpio_write(MYNEWT_VAL(LCD_D2_PIN), PIN(b, 2));
    hal_gpio_write(MYNEWT_VAL(LCD_D3_PIN), PIN(b, 3));
    hal_gpio_write(MYNEWT_VAL(LCD_D4_PIN), PIN(b, 4));
    hal_gpio_write(MYNEWT_VAL(LCD_D5_PIN), PIN(b, 5));
    hal_gpio_write(MYNEWT_VAL(LCD_D6_PIN), PIN(b, 6));
    hal_gpio_write(MYNEWT_VAL(LCD_D7_PIN), PIN(b, 7));
    hal_gpio_write(MYNEWT_VAL(LCD_WR_PIN), 0);
    hal_gpio_write(MYNEWT_VAL(LCD_WR_PIN), 1);
}
#endif

void
lcd_itf_write_bytes(const uint8_t *bytes, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        LCD_ITF_8080_WRITE_BYTE(*bytes++);
    }
}

void
lcd_itf_write_color_data(const void *pixels, size_t size)
{
    const uint16_t *data = (const uint16_t *)pixels;

    LCD_DC_PIN_DATA();
    LCD_CS_PIN_ACTIVE();
    if (LV_COLOR_16_SWAP == 0) {
        for (size_t i = 0; i < size; i += 2, data++) {
            LCD_ITF_8080_WRITE_BYTE(*data >> 8);
            LCD_ITF_8080_WRITE_BYTE((uint8_t)*data);
        }
    } else {
        lcd_itf_write_bytes((const uint8_t *)pixels, size);
    }
    LCD_CS_PIN_INACTIVE();
}

void
lcd_ift_write_cmd(const uint8_t *cmd, int cmd_length)
{
    LCD_DC_PIN_COMMAND();
    LCD_CS_PIN_ACTIVE();
    LCD_ITF_8080_WRITE_BYTE(*cmd);
    if (cmd_length > 1) {
        LCD_DC_PIN_DATA();
        lcd_itf_write_bytes(cmd + 1, cmd_length - 1);
    }
    LCD_CS_PIN_INACTIVE();
}


void
lcd_itf_init(void)
{
    hal_gpio_init_out(MYNEWT_VAL(LCD_DC_PIN), 0);
    if (MYNEWT_VAL(LCD_CS_PIN) >= 0) {
        hal_gpio_init_out(MYNEWT_VAL(LCD_CS_PIN), 1);
    }
    if (MYNEWT_VAL(LCD_RD_PIN) >= 0) {
        hal_gpio_init_out(MYNEWT_VAL(LCD_RD_PIN), 1);
    }
    if (MYNEWT_VAL(LCD_RESET_PIN) >= 0) {
        hal_gpio_init_out(MYNEWT_VAL(LCD_RESET_PIN), 1);
    }
    hal_gpio_init_out(MYNEWT_VAL(LCD_WR_PIN), 1);
    hal_gpio_init_out(MYNEWT_VAL(LCD_DC_PIN), 0);
    hal_gpio_init_out(MYNEWT_VAL(LCD_D0_PIN), 0);
    hal_gpio_init_out(MYNEWT_VAL(LCD_D1_PIN), 0);
    hal_gpio_init_out(MYNEWT_VAL(LCD_D2_PIN), 0);
    hal_gpio_init_out(MYNEWT_VAL(LCD_D3_PIN), 0);
    hal_gpio_init_out(MYNEWT_VAL(LCD_D4_PIN), 0);
    hal_gpio_init_out(MYNEWT_VAL(LCD_D5_PIN), 0);
    hal_gpio_init_out(MYNEWT_VAL(LCD_D6_PIN), 0);
    hal_gpio_init_out(MYNEWT_VAL(LCD_D7_PIN), 0);
}
