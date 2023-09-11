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

#ifndef LCD_ITF_8080_WRITE_WORD

#if MYNEWT_VAL_CHOICE(LCD_ITF, 8080_II_8_bit)

#define LCD_ITF_8080_WRITE_WORD(w) do {         \
        LCD_ITF_8080_WRITE_BYTE(w >> 8);        \
        LCD_ITF_8080_WRITE_BYTE((uint8_t)w);    \
} while (0)

#else

#define LCD_ITF_8080_WRITE_WORD(n) lcd_itf_8080_write_word(n)

void
lcd_itf_8080_write_word(uint16_t w)
{
    hal_gpio_write(MYNEWT_VAL(LCD_D0_PIN), PIN(w, 0));
    hal_gpio_write(MYNEWT_VAL(LCD_D1_PIN), PIN(w, 1));
    hal_gpio_write(MYNEWT_VAL(LCD_D2_PIN), PIN(w, 2));
    hal_gpio_write(MYNEWT_VAL(LCD_D3_PIN), PIN(w, 3));
    hal_gpio_write(MYNEWT_VAL(LCD_D4_PIN), PIN(w, 4));
    hal_gpio_write(MYNEWT_VAL(LCD_D5_PIN), PIN(w, 5));
    hal_gpio_write(MYNEWT_VAL(LCD_D6_PIN), PIN(w, 6));
    hal_gpio_write(MYNEWT_VAL(LCD_D7_PIN), PIN(w, 7));
    hal_gpio_write(MYNEWT_VAL(LCD_D8_PIN), PIN(w, 8));
    hal_gpio_write(MYNEWT_VAL(LCD_D9_PIN), PIN(w, 9));
    hal_gpio_write(MYNEWT_VAL(LCD_D10_PIN), PIN(w, 10));
    hal_gpio_write(MYNEWT_VAL(LCD_D11_PIN), PIN(w, 11));
    hal_gpio_write(MYNEWT_VAL(LCD_D12_PIN), PIN(w, 12));
    hal_gpio_write(MYNEWT_VAL(LCD_D13_PIN), PIN(w, 13));
    hal_gpio_write(MYNEWT_VAL(LCD_D14_PIN), PIN(w, 14));
    hal_gpio_write(MYNEWT_VAL(LCD_D15_PIN), PIN(w, 15));
    hal_gpio_write(MYNEWT_VAL(LCD_WR_PIN), 0);
    hal_gpio_write(MYNEWT_VAL(LCD_WR_PIN), 1);
}
#endif
#endif

void
lcd_itf_write_bytes(const uint8_t *bytes, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        LCD_ITF_8080_WRITE_BYTE(*bytes++);
    }
}

void
lcd_itf_write_color_data(uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2, const void *pixels)
{
    const uint16_t *data = (const uint16_t *)pixels;
    size_t size = (x2 - x1 + 1) * (y2 - y1 + 1) * 2;

    LCD_DC_PIN_DATA();
    LCD_CS_PIN_ACTIVE();
    if (LV_COLOR_16_SWAP == 0) {
        for (size_t i = 0; i < size; i += 2, data++) {
            LCD_ITF_8080_WRITE_WORD(*data);
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
#if MYNEWT_VAL_CHOICE(LCD_ITF, 8080_II_16_bit)
    hal_gpio_init_out(MYNEWT_VAL(LCD_D8_PIN), 0);
    hal_gpio_init_out(MYNEWT_VAL(LCD_D9_PIN), 0);
    hal_gpio_init_out(MYNEWT_VAL(LCD_D10_PIN), 0);
    hal_gpio_init_out(MYNEWT_VAL(LCD_D11_PIN), 0);
    hal_gpio_init_out(MYNEWT_VAL(LCD_D12_PIN), 0);
    hal_gpio_init_out(MYNEWT_VAL(LCD_D13_PIN), 0);
    hal_gpio_init_out(MYNEWT_VAL(LCD_D14_PIN), 0);
    hal_gpio_init_out(MYNEWT_VAL(LCD_D15_PIN), 0);
#endif
}
