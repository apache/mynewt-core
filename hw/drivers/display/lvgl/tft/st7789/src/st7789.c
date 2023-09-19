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
#include <lv_glue.h>
#include <hal/lv_hal_disp.h>
#include <hal/hal_gpio.h>
#include <lcd_itf.h>

#define ST7789_NOP              0x00
#define ST7789_SWRESET          0x01
#define ST7789_RDDID            0x04
#define ST7789_RDDST            0x09
#define ST7789_RDDPM            0x0A
#define ST7789_RDDDMADCTL       0x0B
#define ST7789_RDDCOLMOD        0x0C
#define ST7789_RDDIM            0x0D
#define ST7789_RDDSM            0x0E
#define ST7789_RDDSDR           0x0F

#define ST7789_SLPIN            0x10
#define ST7789_SLPOUT           0x11
#define ST7789_PTLON            0x12
#define ST7789_NORON            0x13

#define ST7789_INVOFF           0x20
#define ST7789_INVON            0x21
#define ST7789_GAMSET           0x26
#define ST7789_DISPOFF          0x28
#define ST7789_DISPON           0x29
#define ST7789_CASET            0x2A
#define ST7789_RASET            0x2B
#define ST7789_RAMWR            0x2C
#define ST7789_RAMRD            0x2E

#define ST7789_PTLAR            0x30
#define ST7789_VSCRDEF          0x33
#define ST7789_TEOFF            0x34
#define ST7789_TEON             0x35
#define ST7789_MADCTL           0x36
#define ST7789_VSCRSADD         0x37
#define ST7789_IDMOFF           0x38
#define ST7789_IDMON            0x39
#define ST7789_COLMOD           0x3A
#define ST7789_RAMWRC           0x3C
#define ST7789_RAMRDC           0x3E
#define ST7789_TESCAN           0x44
#define ST7789_RDTESCAN         0x45
#define ST7789_WRDISBV          0x51
#define ST7789_RDDISBV          0x52
#define ST7789_WRCTRLD          0x53
#define ST7789_RDCTRLD          0x54
#define ST7789_WRCACE           0x55
#define ST7789_RDCABC           0x56
#define ST7789_WRCABCMB         0x5E
#define ST7789_RDCABCMB         0x5F
#define ST7789_RDABCSDR         0x68

#define ST7789_RAMCTRL          0xB0
#define ST7789_RGBCTRL          0xB1
#define ST7789_PORCTRL          0xB2
#define ST7789_FRCTRL1          0xB3

#define ST7789_PARCTRL          0xB5

#define ST7789_GCTRL            0xB7
#define ST7789_GTADJ            0xB8
#define ST7789_DGMEN            0xBA
#define ST7789_VCOMS            0xBB
#define ST7789_POWSAVE          0xBC
#define ST7789_DLPOFFSAVE       0xBD

#define ST7789_LCMCTRL          0xC0
#define ST7789_IDSET            0xC1
#define ST7789_VDVVRHEN         0xC2
#define ST7789_VRHS             0xC3
#define ST7789_VDVSET           0xC4
#define ST7789_VCMOFSET         0xC5
#define ST7789_FRCTR2           0xC6
#define ST7789_CABCCTRL         0xC7
#define ST7789_REGSEL1          0xC8

#define ST7789_REGSEL2          0xCA

#define ST7789_PWMFRSEL         0xCC

#define ST7789_PWCTRL1          0xD0

#define ST7789_VAPVANEN         0xD2

#define ST7789_RDID1            0xDA
#define ST7789_RDID2            0xDB
#define ST7789_RDID3            0xDC
#define ST7789_RDID4            0xDD
#define ST7789_NVFCTR2          0xDE
#define ST7789_CMD2EN           0xDF
#define ST7789_PVGAMCTRL        0xE0
#define ST7789_NVGAMCTRL        0xE1
#define ST7789_DGMLUTR          0xE2
#define ST7789_DGMLUTB          0xE2

#define ST7789_GATECTRL         0xE4
#define ST7789_SPI2EN           0xE7
#define ST7789_PWCTRL2          0xE8
#define ST7789_EQCTRL           0xE9

#define ST7789_PROMCTRL         0xEC

#define ST7789_PROMEN           0xFA

#define ST7789_NVMSET           0xFC

#define ST7789_PROMCAT          0xFE

#define ST7789_MADCTL_MY        0x80
#define ST7789_MADCTL_MX        0x40
#define ST7789_MADCTL_MV        0x20
#define ST7789_MADCTL_ML        0x10
#define ST7789_MADCTL_RGB       0x00
#define ST7789_MADCTL_BGR       0x08

#define ST7789_HOR_RES          MYNEWT_VAL(LVGL_DISPLAY_HORIZONTAL_RESOLUTION)
#define ST7789_VER_RES          MYNEWT_VAL(LVGL_DISPLAY_VERTICAL_RESOLUTION)

void
st7789_rotate(lv_disp_rot_t rotation)
{
    uint8_t madcmd[2] = { ST7789_MADCTL };

    switch (rotation) {
    case LV_DISP_ROT_270:
        madcmd[1] |= ST7789_MADCTL_MV | ST7789_MADCTL_MY | ST7789_MADCTL_ML;
        break;
    case LV_DISP_ROT_180:
        madcmd[1] |= ST7789_MADCTL_MX | ST7789_MADCTL_MY;
        break;
    case LV_DISP_ROT_90:
        madcmd[1] |= ST7789_MADCTL_MX | ST7789_MADCTL_MV;
        break;
    case LV_DISP_ROT_NONE:
        break;
    }
    lcd_ift_write_cmd(madcmd, 2);
}

LCD_SEQUENCE(init_cmds)
        LCD_SEQUENCE_LCD_CS_INACTIVATE(),
        LCD_SEQUENCE_LCD_DC_DATA(),
        1, ST7789_NOP,
#if MYNEWT_VAL(LCD_RESET_PIN) < 0
        1, ST7789_SWRESET,
        LCD_SEQUENCE_DELAY(5),
#else
        LCD_SEQUENCE_LCD_RESET_ACTIVATE(),
        LCD_SEQUENCE_DELAY_US(10),
        LCD_SEQUENCE_LCD_RESET_INACTIVATE(),
        LCD_SEQUENCE_DELAY(5),
#endif
        1, ST7789_SLPOUT,
        2, ST7789_COLMOD, 0x55,
        2, ST7789_MADCTL, 0x00,
        5, ST7789_CASET, 0x00, 0x00, 0x00, 0xEF,
        5, ST7789_RASET, 0x00, 0x00, 0x01, 0x3F,
#if MYNEWT_VAL(ST7789_INVERSION_ON)
        1, ST7789_INVON,
#else
        1, ST7789_INVOFF,
#endif
        1, ST7789_NORON,
        1, ST7789_DISPON,
LCD_SEQUENCE_END

/**
 *
 * Initialize the ST7789 display controller
 */
void
st7789_init(lv_disp_drv_t *driver)
{
    lcd_command_sequence(init_cmds);
}

static void
st7789_drv_update(struct _lv_disp_drv_t *drv)
{
    st7789_rotate(drv->rotated);
}

/* The ST7789 display controller can drive up to 320*240 displays, when using a 240*240 or 240*135
 * displays there's a gap of 80px or 40/52/53px respectively. 52px or 53x offset depends on display orientation.
 * We need to edit the coordinates to take into account those gaps, this is not necessary in all orientations. */
void
st7789_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint8_t cmd[5];

    /* Truncate the area to the screen */
    int32_t offsetx1 = area->x1 < 0 ? 0 : area->x1;
    int32_t offsety1 = area->y1 < 0 ? 0 : area->y1;
    int32_t offsetx2 = area->x2 >= ST7789_HOR_RES ? ST7789_HOR_RES - 1 : area->x2;
    int32_t offsety2 = area->y2 >= ST7789_VER_RES ? ST7789_VER_RES - 1 : area->y2;

#if (CONFIG_LV_TFT_DISPLAY_OFFSETS)
    offsetx1 += CONFIG_LV_TFT_DISPLAY_X_OFFSET;
    offsetx2 += CONFIG_LV_TFT_DISPLAY_X_OFFSET;
    offsety1 += CONFIG_LV_TFT_DISPLAY_Y_OFFSET;
    offsety2 += CONFIG_LV_TFT_DISPLAY_Y_OFFSET;

#elif (ST7789_HOR_RES == 240) && (ST7789_VER_RES == 240)
#if (CONFIG_LV_DISPLAY_ORIENTATION_PORTRAIT)
    offsetx1 += 80;
    offsetx2 += 80;
#elif (CONFIG_LV_DISPLAY_ORIENTATION_LANDSCAPE_INVERTED)
    offsety1 += 80;
    offsety2 += 80;
#endif
#elif (LV_HOR_RES_MAX == 240) && (LV_VER_RES_MAX == 135)
#if (CONFIG_LV_DISPLAY_ORIENTATION_PORTRAIT) || \
    (CONFIG_LV_DISPLAY_ORIENTATION_PORTRAIT_INVERTED)
    offsetx1 += 40;
    offsetx2 += 40;
    offsety1 += 53;
    offsety2 += 53;
#endif
#elif (LV_HOR_RES_MAX == 135) && (LV_VER_RES_MAX == 240)
#if (CONFIG_LV_DISPLAY_ORIENTATION_LANDSCAPE) || \
    (CONFIG_LV_DISPLAY_ORIENTATION_LANDSCAPE_INVERTED)
    offsetx1 += 52;
    offsetx2 += 52;
    offsety1 += 40;
    offsety2 += 40;
#endif
#endif

    /* Column addresses */
    cmd[0] = ST7789_CASET;
    cmd[1] = (uint8_t)(offsetx1 >> 8);
    cmd[2] = (uint8_t)offsetx1;
    cmd[3] = (uint8_t)(offsetx2 >> 8);
    cmd[4] = (uint8_t)offsetx2;
    lcd_ift_write_cmd(cmd, 5);

    /* Page address */
    cmd[0] = ST7789_RASET;
    cmd[1] = (uint8_t)(offsety1 >> 8);
    cmd[2] = (uint8_t)offsety1;
    cmd[3] = (uint8_t)(offsety2 >> 8);
    cmd[4] = (uint8_t)offsety2;
    lcd_ift_write_cmd(cmd, 5);

    cmd[0] = ST7789_RAMWR;
    lcd_ift_write_cmd(cmd, 1);

    lcd_itf_write_color_data(offsetx1, offsetx2, offsety1, offsety2, color_p);

    lv_disp_flush_ready(drv);
}

void
mynewt_lv_drv_init(lv_disp_drv_t *driver)
{
    if (MYNEWT_VAL(LCD_BL_PIN) >= 0) {
        hal_gpio_init_out(MYNEWT_VAL(LCD_BL_PIN), MYNEWT_VAL(LCD_BL_PIN_ACTIVE_LEVEL));
    }
    if (MYNEWT_VAL(LCD_RESET_PIN) >= 0) {
        hal_gpio_init_out(MYNEWT_VAL(LCD_RESET_PIN), 1);
    }
    lcd_itf_init();
    driver->flush_cb = st7789_flush;
    driver->drv_update_cb = st7789_drv_update;
    driver->hor_res = ST7789_HOR_RES;
    driver->ver_res = ST7789_VER_RES;

    st7789_init(driver);
}
