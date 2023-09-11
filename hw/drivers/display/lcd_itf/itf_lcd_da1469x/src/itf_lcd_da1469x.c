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
#include <bus/drivers/spi_common.h>

#include <lv_conf.h>
#include <misc/lv_color.h>
#include <lcd_itf.h>

static uint16_t display_resx;
static uint16_t display_resy;

void
lcd_itf_write_color_data(uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2, const void *pixels)
{
    uint16_t width = 1 + x2 - x1;
    uint16_t height = 1 + y2 - y1;

    LCDC->LCDC_LAYER0_BASEADDR_REG = (uint32_t)pixels;
    LCDC->LCDC_RESXY_REG = (width << 16) | height;
    LCDC->LCDC_LAYER0_OFFSETX_REG = 0;
    LCDC->LCDC_LAYER0_SIZEXY_REG = (width << 16) | height;
    LCDC->LCDC_LAYER0_RESXY_REG = (width << 16) | height;
    LCDC->LCDC_LAYER0_STRIDE_REG = width * sizeof(lv_color_t);
    LCDC->LCDC_LAYER0_MODE_REG = LCDC_LCDC_LAYER0_MODE_REG_LCDC_L0_EN_Msk | 5;
    LCDC->LCDC_MODE_REG |= LCDC_LCDC_MODE_REG_LCDC_SFRAME_UPD_Msk;
    /* Dummy read, it looks like busy status may not be there on the first read */
    (void)LCDC->LCDC_STATUS_REG;
    while ((LCDC->LCDC_STATUS_REG & LCDC_LCDC_STATUS_REG_LCDC_FRAMEGEN_BUSY_Msk));
}

void
lcd_ift_write_cmd(const uint8_t *cmd, int cmd_length)
{
    int i;
    uint32_t cmd_bit = LCDC_LCDC_DBIB_CMD_REG_LCDC_DBIB_CMD_SEND_Msk;

    for (i = 0; i < cmd_length; ++i) {
        if (((LCDC->LCDC_DBIB_CFG_REG & (LCDC_LCDC_DBIB_CFG_REG_LCDC_DBIB_SPI_HOLD_Msk |
                                         LCDC_LCDC_DBIB_CFG_REG_LCDC_DBIB_SPI4_EN_Msk)) !=
             LCDC_LCDC_DBIB_CFG_REG_LCDC_DBIB_SPI4_EN_Msk) ||
            (LCDC->LCDC_DBIB_CMD_REG & LCDC_LCDC_DBIB_CMD_REG_LCDC_DBIB_CMD_SEND_Msk) == cmd_bit) {
            while (LCDC->LCDC_STATUS_REG & LCDC_LCDC_STATUS_REG_LCDC_DBIB_CMD_FIFO_FULL_Msk);
        } else {
            while (LCDC->LCDC_STATUS_REG & LCDC_LCDC_STATUS_REG_LCDC_DBIB_CMD_PENDING_Msk);
        }

        LCDC->LCDC_DBIB_CMD_REG = cmd_bit | cmd[i];
        cmd_bit = 0;
    }
}


void
lcd_itf_init(void)
{
    CRG_SYS->CLK_SYS_REG |= CRG_SYS_CLK_SYS_REG_LCD_ENABLE_Msk |
                            CRG_SYS_CLK_SYS_REG_LCD_CLK_SEL_Msk;

    /* Device without LCD controller DA14691 does not have magic number at this address */
    if (LCDC->LCDC_IDREG_REG != 0x87452365) {
        assert(0);
        return;
    }

    display_resx = 240;
    display_resy = 320;

    if (MYNEWT_VAL(LCD_DC_PIN) >= 0) {
        mcu_gpio_set_pin_function(MYNEWT_VAL(LCD_DC_PIN), MCU_GPIO_MODE_OUTPUT, MCU_GPIO_FUNC_LCD_SPI_DC);
    }
    if (MYNEWT_VAL(LCD_CS_PIN) >= 0) {
        mcu_gpio_set_pin_function(MYNEWT_VAL(LCD_CS_PIN), MCU_GPIO_MODE_OUTPUT, MCU_GPIO_FUNC_LCD_SPI_EN);
    }
    if (MYNEWT_VAL(LCD_MOSI_PIN) >= 0) {
        mcu_gpio_set_pin_function(MYNEWT_VAL(LCD_MOSI_PIN), MCU_GPIO_MODE_OUTPUT, MCU_GPIO_FUNC_LCD_SPI_DO);
    }
    if (MYNEWT_VAL(LCD_SCLK_PIN) >= 0) {
        mcu_gpio_set_pin_function(MYNEWT_VAL(LCD_SCLK_PIN), MCU_GPIO_MODE_OUTPUT, MCU_GPIO_FUNC_LCD_SPI_CLK);
    }

    uint32_t clk_sys_reg = CRG_SYS->CLK_SYS_REG &
                           ~(CRG_SYS_CLK_SYS_REG_LCD_RESET_REQ_Msk | CRG_SYS_CLK_SYS_REG_LCD_CLK_SEL_Msk |
                             CRG_SYS_CLK_SYS_REG_LCD_ENABLE_Msk);
    CRG_SYS->CLK_SYS_REG = clk_sys_reg | CRG_SYS_CLK_SYS_REG_LCD_RESET_REQ_Msk;
    CRG_SYS->CLK_SYS_REG = clk_sys_reg | CRG_SYS_CLK_SYS_REG_LCD_ENABLE_Msk | CRG_SYS_CLK_SYS_REG_LCD_CLK_SEL_Msk;

    LCDC->LCDC_CLKCTRL_REG = (4 << 8) | 2; /* TODO: calculate divider */
    LCDC->LCDC_CLKCTRL_REG = 0x401;
    LCDC->LCDC_MODE_REG = 0;
    while (LCDC->LCDC_STATUS_REG & LCDC_LCDC_STATUS_REG_LCDC_DBIB_CMD_FIFO_FULL_Msk);
    LCDC->LCDC_DBIB_CFG_REG =
        LCDC_LCDC_DBIB_CFG_REG_LCDC_DBIB_SPI4_EN_Msk |
        LCDC_LCDC_DBIB_CFG_REG_LCDC_DBIB_DMA_EN_Msk |
        LCDC_LCDC_DBIB_CFG_REG_LCDC_DBIB_TE_DIS_Msk |
        LCDC_LCDC_DBIB_CFG_REG_LCDC_DBIB_RESX_Msk |
        LCDC_LCDC_DBIB_CFG_REG_LCDC_DBIB_SPI_CPHA_Msk |
        LCDC_LCDC_DBIB_CFG_REG_LCDC_DBIB_SPI_CPOL_Msk |
        0x12 /* RGB565 */;
    /* RGB565 */
    LCDC->LCDC_RESXY_REG = (240 << 16) | 320;
    LCDC->LCDC_FRONTPORCHXY_REG = (240 << 16) | 320;
    LCDC->LCDC_BLANKINGXY_REG = (240 << 16) | 321;
    LCDC->LCDC_BACKPORCHXY_REG = (240 << 16) | 321;
    LCDC->LCDC_LAYER0_MODE_REG = LCDC_LCDC_LAYER0_MODE_REG_LCDC_L0_EN_Msk | 5;
}
