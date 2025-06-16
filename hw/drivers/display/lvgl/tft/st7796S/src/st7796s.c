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

/* Magic numbers used to lock/unlock command settings */
#define ST7796S_UNLOCK_1         0xC3
#define ST7796S_UNLOCK_2         0x96

#define ST7796S_LOCK_1           0x3C
#define ST7796S_LOCK_2           0x69

#define ST7796S_NOP              0x00
#define ST7796S_SWRESET          0x01
#define ST7796S_RDDID            0x04
#define ST7796S_RDDST            0x09
#define ST7796S_RDDPM            0x0A
#define ST7796S_RDDDMADCTL       0x0B
#define ST7796S_RDDCOLMOD        0x0C
#define ST7796S_RDDIM            0x0D
#define ST7796S_RDDSM            0x0E
#define ST7796S_RDDSDR           0x0F

#define ST7796S_SLPIN            0x10
#define ST7796S_SLPOUT           0x11
#define ST7796S_PTLON            0x12
#define ST7796S_NORON            0x13

#define ST7796S_INVOFF           0x20
#define ST7796S_INVON            0x21
#define ST7796S_DISPOFF          0x28
#define ST7796S_DISPON           0x29
#define ST7796S_CASET            0x2A
#define ST7796S_RASET            0x2B
#define ST7796S_RAMWR            0x2C
#define ST7796S_RAMRD            0x2E

#define ST7796S_PTLAR            0x30
#define ST7796S_VSCRDEF          0x33
#define ST7796S_TEOFF            0x34
#define ST7796S_TEON             0x35
#define ST7796S_MADCTL           0x36
#define ST7796S_VSCRSADD         0x37
#define ST7796S_IDMOFF           0x38
#define ST7796S_IDMON            0x39
#define ST7796S_COLMOD           0x3A
#define ST7796S_WRMEMC           0x3C
#define ST7796S_RDMEMC           0x3E
#define ST7796S_STE              0x44
#define ST7796S_GSCAN            0x45
#define ST7796S_WRDISBV          0x51
#define ST7796S_RDDISBV          0x52
#define ST7796S_WRCTRLD          0x53
#define ST7796S_RDCTRLD          0x54
#define ST7796S_WRCACE           0x55
#define ST7796S_RDCABC           0x56
#define ST7796S_WRCABCMB         0x5E
#define ST7796S_RDCABCMB         0x5F
#define ST7796S_RDABCSDR         0x68

#define ST7796S_IFMODE           0xB0
#define ST7796S_FRMCTR1          0xB1
#define ST7796S_FRMCTR2          0xB2
#define ST7796S_FRMCTR3          0xB3
#define ST7796S_DIC              0xB4
#define ST7796S_BPC              0xB5
#define ST7796S_DFC              0xB6
#define ST7796S_EM               0xB7

#define ST7796S_DGMEN            0xBA
#define ST7796S_VCOMS            0xBB
#define ST7796S_POWSAVE          0xBC
#define ST7796S_DLPOFFSAVE       0xBD

#define ST7796S_PWR1             0xC0
#define ST7796S_PWR2             0xC1
#define ST7796S_PWR3             0xC2

#define ST7796S_VCMPCTL          0xC5

#define ST7796S_NVMADW           0xD0
#define ST7796S_NVMBPROG         0xD1
#define ST7796S_NVM              0xD2
#define ST7796S_RDID4            0xD3

#define ST7796S_PGC              0xE0
#define ST7796S_NGC              0xE1
#define ST7796S_DGC1             0xE2
#define ST7796S_DGC2             0xE3

#define ST7796S_DOCA             0xE8

#define ST7796S_PROMCTRL         0xEC

#define ST7796S_CSCON            0xF0
#define ST7796S_PROMEN           0xFA

#define ST7796S_NVMSET           0xFC

#define ST7796S_PROMCAT          0xFE

#define ST7796S_MADCTL_MY        0x80
#define ST7796S_MADCTL_MX        0x40
#define ST7796S_MADCTL_MV        0x20
#define ST7796S_MADCTL_ML        0x10
#define ST7796S_MADCTL_RGB       0x00
#define ST7796S_MADCTL_BGR       0x08
#define ST7796S_MADCTL_0         ST7796S_MADCTL_MX
#define ST7796S_MADCTL_90        (ST7796S_MADCTL_MV)
#define ST7796S_MADCTL_180       (ST7796S_MADCTL_MY | ST7796S_MADCTL_ML)
#define ST7796S_MADCTL_270       (ST7796S_MADCTL_MY | ST7796S_MADCTL_ML | ST7796S_MADCTL_MV | ST7796S_MADCTL_MX)

#define ST7796S_HOR_RES          MYNEWT_VAL(LVGL_DISPLAY_HORIZONTAL_RESOLUTION)
#define ST7796S_VER_RES          MYNEWT_VAL(LVGL_DISPLAY_VERTICAL_RESOLUTION)

void
st7789_rotate(lv_disp_rot_t rotation)
{
    uint8_t madcmd[2] = { ST7796S_MADCTL, ST7796S_MADCTL_BGR };

    switch (rotation) {
    case LV_DISP_ROT_270:
        madcmd[1] |= ST7796S_MADCTL_270;
        break;
    case LV_DISP_ROT_180:
        madcmd[1] |= ST7796S_MADCTL_180;
        break;
    case LV_DISP_ROT_90:
        madcmd[1] |= ST7796S_MADCTL_90;
        break;
    case LV_DISP_ROT_NONE:
        madcmd[1] |= ST7796S_MADCTL_0;
        break;
    }
    lcd_ift_write_cmd(madcmd, 2);
}

LCD_SEQUENCE(init_cmds)
        LCD_SEQUENCE_LCD_CS_INACTIVATE(),
        LCD_SEQUENCE_LCD_DC_DATA(),
        1, ST7796S_NOP,
#if MYNEWT_VAL(LCD_RESET_PIN) < 0
        1, ST7796S_SWRESET,
        LCD_SEQUENCE_DELAY(5),
#else
        LCD_SEQUENCE_LCD_RESET_ACTIVATE(),
        LCD_SEQUENCE_DELAY_US(100),
        LCD_SEQUENCE_LCD_RESET_INACTIVATE(),
        LCD_SEQUENCE_DELAY(5),
#endif
        2, ST7796S_CSCON, ST7796S_UNLOCK_1,
        2, ST7796S_CSCON, ST7796S_UNLOCK_2,
        2, ST7796S_MADCTL, ST7796S_MADCTL_0 | ST7796S_MADCTL_BGR,
        2, ST7796S_COLMOD, 0x55,
        4, ST7796S_DFC, 0x8A, 0x07, 0x3B,
        5, ST7796S_BPC, 2, 3, 0, 4,
        3, ST7796S_FRMCTR1, 0x80, 0x10,
        2, ST7796S_DIC, 0,
        2, ST7796S_EM, 0xc6,
        2, ST7796S_VCMPCTL, 0x24,
        9, ST7796S_DOCA, 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33,
        2, ST7796S_PWR3, 0xA7,
        15, ST7796S_PGC, 0xF0, 0x09, 0x13, 0x12, 0x12, 0x2B, 0x3C, 0x44, 0x4B, 0x1B, 0x18, 0x17, 0x1D, 0x21,
        15, ST7796S_NGC, 0xF0, 0x09, 0x13, 0x0C, 0x0D, 0x27, 0x3B, 0x44, 0x4D, 0x0B, 0x17, 0x17, 0x1D, 0x21,
        2, ST7796S_CSCON, ST7796S_LOCK_1,
        2, ST7796S_CSCON, ST7796S_LOCK_2,
        1, ST7796S_NORON,
#if MYNEWT_VAL(ST7796S_INVERSION_ON)
        1, ST7796S_INVON,
#else
        1, ST7796S_INVOFF,
#endif
        1, ST7796S_SLPOUT,
        LCD_SEQUENCE_DELAY(15),
        1, ST7796S_DISPON,
LCD_SEQUENCE_END

/**
 *
 * Initialize the ST7789 display controller
 */
void
st7789_init(lv_disp_drv_t *driver)
{
    (void)driver;

    lcd_command_sequence(init_cmds);
}

static void
st7789_drv_update(struct _lv_disp_drv_t *drv)
{
    st7789_rotate(drv->rotated);
}

void
st7789_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint8_t cmd[5];
    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t hor_res = lv_disp_get_hor_res(disp);
    lv_coord_t ver_res = lv_disp_get_ver_res(disp);

    if (area->x2 < 0 || area->y2 < 0 || area->x1 >= hor_res || area->y1 >= ver_res) {
        lv_disp_flush_ready(drv);
        return;
    }

    /* Truncate the area to the screen */
    int32_t act_x1 = area->x1 < 0 ? 0 : area->x1;
    int32_t act_y1 = area->y1 < 0 ? 0 : area->y1;
    int32_t act_x2 = area->x2 >= hor_res ? hor_res - 1 : area->x2;
    int32_t act_y2 = area->y2 >= ver_res ? ver_res - 1 : area->y2;

    /* Column addresses */
    cmd[0] = ST7796S_CASET;
    cmd[1] = (uint8_t)(act_x1 >> 8);
    cmd[2] = (uint8_t)act_x1;
    cmd[3] = (uint8_t)(act_x2 >> 8);
    cmd[4] = (uint8_t)act_x2;
    lcd_ift_write_cmd(cmd, 5);

    /* Page address */
    cmd[0] = ST7796S_RASET;
    cmd[1] = (uint8_t)(act_y1 >> 8);
    cmd[2] = (uint8_t)act_y1;
    cmd[3] = (uint8_t)(act_y2 >> 8);
    cmd[4] = (uint8_t)act_y2;
    lcd_ift_write_cmd(cmd, 5);

    cmd[0] = ST7796S_RAMWR;
    lcd_ift_write_cmd(cmd, 1);

    lcd_itf_write_color_data(act_x1, act_x2, act_y1, act_y2, color_p);

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
    driver->hor_res = ST7796S_HOR_RES;
    driver->ver_res = ST7796S_VER_RES;

    st7789_init(driver);
}
