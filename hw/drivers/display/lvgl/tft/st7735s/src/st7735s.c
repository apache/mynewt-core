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
#include <lv_glue.h>
#include <hal/lv_hal_disp.h>
#include <lcd_itf.h>

#define ST7735S_TFTWIDTH        128
#define ST7735S_TFTHEIGHT       160

#define ST7735S_NOP             0x00
#define ST7735S_SWRESET         0x01
#define ST7735S_RDDID           0x04
#define ST7735S_RDDST           0x09
#define ST7735S_RDDPM           0x0A
#define ST7735S_RDDDMADCTL      0x0B
#define ST7735S_RDDCOLMOD       0x0C
#define ST7735S_RDDIM           0x0D
#define ST7735S_RDDSM           0x0E
#define ST7735S_RDDSDR          0x0F

#define ST7735S_SLPIN           0x10
#define ST7735S_SLPOUT          0x11
#define ST7735S_PTLON           0x12
#define ST7735S_NORON           0x13

#define ST7735S_INVOFF          0x20
#define ST7735S_INVON           0x21
#define ST7735S_GAMSET          0x26
#define ST7735S_DISPOFF         0x28
#define ST7735S_DISPON          0x29
#define ST7735S_CASET           0x2A
#define ST7735S_RASET           0x2B
#define ST7735S_RAMWR           0x2C
#define ST7735S_RAMRD           0x2E

#define ST7735S_PTLAR           0x30
#define ST7735S_SCRLAR          0x33
#define ST7735S_TEOFF           0x34
#define ST7735S_TEON            0x35
#define ST7735S_MADCTL          0x36
#define ST7735S_VSCSAD          0x37
#define ST7735S_IDMOFF          0x38
#define ST7735S_IDMON           0x39
#define ST7735S_COLMOD          0x3A

#define ST7735S_FRMCTR1         0xB1
#define ST7735S_FRMCTR2         0xB2
#define ST7735S_FRMCTR3         0xB3
#define ST7735S_INVCTR          0xB4

#define ST7735S_PWCTR1          0xC0
#define ST7735S_PWCTR2          0xC1
#define ST7735S_PWCTR3          0xC2
#define ST7735S_PWCTR4          0xC3
#define ST7735S_PWCTR5          0xC4
#define ST7735S_VMCTR1          0xC5
#define ST7735S_VMOFCTR         0xC7

#define ST7735S_WRID2           0xD1
#define ST7735S_WRID3           0xD2
#define ST7735S_NVCTR1          0xD9
#define ST7735S_RDID1           0xDA
#define ST7735S_RDID2           0xDB
#define ST7735S_RDID3           0xDC
#define ST7735S_RDID4           0xDD
#define ST7735S_NVFCTR2         0xDE
#define ST7735S_NVFCTR3         0xDF

#define ST7735S_GMCTRP1         0xE0
#define ST7735S_GMCTRN1         0xE1

#define ST7735S_GCV             0xFC

#define ST7735S_MADCTL_MY       0x80
#define ST7735S_MADCTL_MX       0x40
#define ST7735S_MADCTL_MV       0x20
#define ST7735S_MADCTL_ML       0x10
#define ST7735S_MADCTL_RGB      0x00
#define ST7735S_MADCTL_BGR      0x08

#define ST7735S_HOR_RES       ST7735S_TFTWIDTH
#define ST7735S_VER_RES       ST7735S_TFTHEIGHT

void
st7735s_rotate(lv_disp_rot_t rotation)
{
    uint8_t madctl[2] = {ST7735S_MADCTL, 0};

    switch (rotation) {
    case LV_DISP_ROT_270:
        madctl[1] |= ST7735S_MADCTL_MV | ST7735S_MADCTL_MY | ST7735S_MADCTL_ML;
        break;
    case LV_DISP_ROT_180:
        madctl[1] |= ST7735S_MADCTL_MX | ST7735S_MADCTL_MY;
        break;
    case LV_DISP_ROT_90:
        madctl[1] |= ST7735S_MADCTL_MX | ST7735S_MADCTL_MV;
        break;
    case LV_DISP_ROT_NONE:
        break;
    }
    lcd_ift_write_cmd(madctl, 2);
}

LCD_SEQUENCE(init_cmds)
        LCD_SEQUENCE_LCD_CS_INACTIVATE(),
        LCD_SEQUENCE_LCD_DC_DATA(),
#if MYNEWT_VAL(LCD_RESET_PIN) >= 0
        LCD_SEQUENCE_LCD_RESET_ACTIVATE(),
        LCD_SEQUENCE_DELAY_US(10),
        LCD_SEQUENCE_LCD_RESET_INACTIVATE(),
        LCD_SEQUENCE_DELAY(5),
#endif
        1, ST7735S_NOP,
#if MYNEWT_VAL(LCD_RESET_PIN) < 0
        1, ILI9486_SWRESET,
        LCD_SEQUENCE_DELAY(5),
#endif
        1, ST7735S_SLPOUT,
        LCD_SEQUENCE_DELAY(5),
        4, ST7735S_FRMCTR1, 0x01, 0x2C, 0x2D,
        4, ST7735S_FRMCTR2, 0x01, 0x2C, 0x2D,
        7, ST7735S_FRMCTR2, 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D,
        2, ST7735S_INVCTR, 0x07,
        4, ST7735S_PWCTR1, 0xA2, 0x02, 0x84,
        2, ST7735S_PWCTR2, 0xC5,
        3, ST7735S_PWCTR3, 0x0A, 0x00,
        3, ST7735S_PWCTR4, 0x8A, 0x2A,
        3, ST7735S_PWCTR5, 0x8A, 0xEE,
        2, ST7735S_VMCTR1, 0x0E,
#if invert_colors
        1, ST7735S_INVON,
#else
        1, ST7735S_INVOFF,
#endif
        2, ST7735S_COLMOD, 0x05,
        17, ST7735S_GMCTRP1, 0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d,
            0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10,
        17, ST7735S_GMCTRN1, 0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
            0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10,
        1, ST7735S_NORON,
        LCD_SEQUENCE_DELAY(10),
        2, ST7735S_MADCTL, 0,
        LCD_SEQUENCE_DELAY(100),
        1, ST7735S_DISPON,
LCD_SEQUENCE_END

/**
 * Initialize the ST7735S display controller
 */
void
st7735s_init(lv_disp_drv_t *driver)
{
    lcd_command_sequence(init_cmds);
}

static void
st7735s_drv_update(struct _lv_disp_drv_t *drv)
{
    st7735s_rotate(drv->rotated);
}

void
st7735s_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint8_t cmd[5];

    if (area->x2 < 0 || area->y2 < 0 || area->x1 >= ST7735S_HOR_RES || area->y1 >= ST7735S_VER_RES) {
        lv_disp_flush_ready(drv);
        return;
    }

    /* Truncate the area to the screen */
    int32_t act_x1 = area->x1 < 0 ? 0 : area->x1;
    int32_t act_y1 = area->y1 < 0 ? 0 : area->y1;
    int32_t act_x2 = area->x2 >= ST7735S_HOR_RES ? ST7735S_HOR_RES - 1 : area->x2;
    int32_t act_y2 = area->y2 >= ST7735S_VER_RES ? ST7735S_VER_RES - 1 : area->y2;

    /* Column address */
    cmd[0] = ST7735S_CASET;
    cmd[1] = (uint8_t)(act_x1 >> 8);
    cmd[2] = (uint8_t)act_x1;
    cmd[3] = (uint8_t)(act_x2 >> 8);
    cmd[4] = (uint8_t)act_x2;
    lcd_ift_write_cmd(cmd, 5);

    /* Page address */
    cmd[0] = ST7735S_RASET;
    cmd[1] = (uint8_t)(act_y1 >> 8);
    cmd[2] = (uint8_t)act_y1;
    cmd[3] = (uint8_t)(act_y2 >> 8);
    cmd[4] = (uint8_t)act_y2;
    lcd_ift_write_cmd(cmd, 5);

    cmd[0] = ST7735S_RAMWR;
    lcd_ift_write_cmd(cmd, 1);

    lcd_itf_write_color_data(act_x1, act_x2, act_y1, act_y2, color_p);

    lv_disp_flush_ready(drv);
}

void
mynewt_lv_drv_init(lv_disp_drv_t *driver)
{
    if (MYNEWT_VAL(LCD_BL_PIN) >= 0) {
        hal_gpio_init_out(MYNEWT_VAL(LCD_BL_PIN), 1);
    }
    if (MYNEWT_VAL(LCD_RESET_PIN) >= 0) {
        hal_gpio_init_out(MYNEWT_VAL(LCD_RESET_PIN), 1);
    }
    lcd_itf_init();
    driver->flush_cb = st7735s_flush;
    driver->drv_update_cb = st7735s_drv_update;
    driver->hor_res = ST7735S_TFTWIDTH;
    driver->ver_res = ST7735S_TFTHEIGHT;

    st7735s_init(driver);
}
