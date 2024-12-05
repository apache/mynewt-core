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
#include <mcu/stm32_hal.h>
#include <mcu/mcu.h>

#include <hal/lv_hal_indev.h>
#include <lcd_itf.h>

static inline void
lcd_itf_8080_write_word(uint16_t w)
{
    *(uint16_t *)(0x60000000) = w;
}

#define LCD_ITF_8080_WRITE_BYTE(n) lcd_itf_8080_write_word(n)
#define LCD_ITF_8080_WRITE_WORD(n) lcd_itf_8080_write_word(n)

void
lcd_itf_write_bytes(const uint8_t *bytes, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        LCD_ITF_8080_WRITE_BYTE(*bytes++);
    }
}

void
lcd_itf_write_color_data(uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2, void *pixels)
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
    if (MYNEWT_VAL(LCD_RESET_PIN) >= 0) {
        hal_gpio_init_out(MYNEWT_VAL(LCD_RESET_PIN), 1);
    }
    hal_gpio_init_af(STM32_FSMC_NWE, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);
    hal_gpio_init_af(STM32_FSMC_NOE, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);
    hal_gpio_init_af(STM32_FSMC_D0, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);
    hal_gpio_init_af(STM32_FSMC_D1, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);
    hal_gpio_init_af(STM32_FSMC_D2, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);
    hal_gpio_init_af(STM32_FSMC_D3, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);
    hal_gpio_init_af(STM32_FSMC_D4, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);
    hal_gpio_init_af(STM32_FSMC_D5, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);
    hal_gpio_init_af(STM32_FSMC_D6, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);
    hal_gpio_init_af(STM32_FSMC_D7, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);
    hal_gpio_init_af(STM32_FSMC_D8, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);
    hal_gpio_init_af(STM32_FSMC_D9, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);
    hal_gpio_init_af(STM32_FSMC_D10, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);
    hal_gpio_init_af(STM32_FSMC_D11, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);
    hal_gpio_init_af(STM32_FSMC_D12, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);
    hal_gpio_init_af(STM32_FSMC_D13, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);
    hal_gpio_init_af(STM32_FSMC_D14, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);
    hal_gpio_init_af(STM32_FSMC_D15, GPIO_AF12_FSMC, HAL_GPIO_PULL_NONE, 0);

    __HAL_RCC_FSMC_CLK_ENABLE();
    FSMC_Bank1->BTCR[0] = FSMC_BCR1_WREN_Msk | (1 << FSMC_BCR1_MWID_Pos) |
                          (0 << FSMC_BCR1_MTYP_Pos) | FSMC_BCR1_MBKEN_Msk;
    FSMC_Bank1->BTCR[1] = 0x00100200;
}
