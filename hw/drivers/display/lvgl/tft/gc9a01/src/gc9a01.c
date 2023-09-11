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

#include <lv_glue.h>
#include <bsp/bsp.h>
#include <hal/lv_hal_disp.h>
#include <hal/hal_gpio.h>
#include <lcd_itf.h>

#define GC9A01_TFTWIDTH        240
#define GC9A01_TFTHEIGHT       240

#define GC9A01_RDDID           0x04
#define GC9A01_RDDST           0x09

#define GC9A01_SLPIN           0x10
#define GC9A01_SLPOUT          0x11
#define GC9A01_PTLON           0x12
#define GC9A01_NORON           0x13

#define GC9A01_INVOFF          0x20
#define GC9A01_INVON           0x21

#define GC9A01_DISPOFF         0x28
#define GC9A01_DISPON          0x29
#define GC9A01_CASET           0x2A
#define GC9A01_RASET           0x2B
#define GC9A01_RAMWR           0x2C

#define GC9A01_PTLAR           0x30
#define GC9A01_SCRLAR          0x33
#define GC9A01_TEOFF           0x34
#define GC9A01_TEON            0x35
#define GC9A01_MADCTL          0x36
#define GC9A01_VSCSAD          0x37
#define GC9A01_IDMOFF          0x38
#define GC9A01_IDMON           0x39
#define GC9A01_COLMOD          0x3A
#define GC9A01_WRMRCON         0x3C

#define GC9A01_SETTSL          0x44
#define GC9A01_GETSL           0x45

#define GC9A01_WRDBRINS        0x51
#define GC9A01_WRCTRLDSP       0x53


#define GC9A01_PWCTR7          0xA7

#define GC9A01_RGBISC          0xB0
#define GC9A01_BPCTRL          0xB5
#define GC9A01_DISFUNCTRL      0xB6
#define GC9A01_TEAREFFCTRL     0xBA

#define GC9A01_PWCTR1          0xC1
#define GC9A01_PWCTR2          0xC3
#define GC9A01_PWCTR3          0xC4
#define GC9A01_PWCTR4          0xC9
#define GC9A01_VMCTR1          0xC5
#define GC9A01_VMOFCTR         0xC7

#define GC9A01_RDID1           0xDA
#define GC9A01_RDID2           0xDB
#define GC9A01_RDID3           0xDC

#define GC9A01_FRAMERATE       0xE8
#define GC9A01_SPI2DATACTRL    0xE9

#define GC9A01_GCV             0xFC

#define GC9A01_IREN1           0xFE
#define GC9A01_IREN2           0xEF

#define GC9A01_SETGAMMA1       0xF0
#define GC9A01_SETGAMMA2       0xF1
#define GC9A01_SETGAMMA3       0xF2
#define GC9A01_SETGAMMA4       0xF3

#define GC9A01_IFCTRL          0xF6

#define GC9A01_MADCTL_MY       0x80
#define GC9A01_MADCTL_MX       0x40
#define GC9A01_MADCTL_MV       0x20
#define GC9A01_MADCTL_ML       0x10
#define GC9A01_MADCTL_RGB      0x00
#define GC9A01_MADCTL_BGR      0x08

#define GC9A01_HOR_RES       GC9A01_TFTWIDTH
#define GC9A01_VER_RES       GC9A01_TFTHEIGHT

static const uint8_t madctl[] = {
    GC9A01_MADCTL_MX,
    GC9A01_MADCTL_MV | GC9A01_MADCTL_MY | GC9A01_MADCTL_ML,
    GC9A01_MADCTL_MY,
    GC9A01_MADCTL_MX | GC9A01_MADCTL_MV,
};

void
gc9a01_rotate(lv_disp_rot_t rotation)
{
    uint8_t madcmd[2] = { GC9A01_MADCTL, GC9A01_MADCTL_BGR | madctl[rotation] };

    lcd_ift_write_cmd(madcmd, 2);
}

LCD_SEQUENCE(init_cmds)
    LCD_SEQUENCE_LCD_CS_INACTIVATE(),
    LCD_SEQUENCE_LCD_DC_DATA(),
    1, 0,
#if MYNEWT_VAL(LCD_RESET_PIN) < 0
    1, 1,
#else
    LCD_SEQUENCE_LCD_RESET_ACTIVATE(),
    LCD_SEQUENCE_DELAY_US(10),
    LCD_SEQUENCE_LCD_RESET_INACTIVATE(),
#endif
    LCD_SEQUENCE_DELAY(100),
    /* command length, command, args */
    1, 0x01,
    2, 0xEB, 0x14,
    1, GC9A01_IREN1,
    1, GC9A01_IREN2,
    2, 0xEB, 0x14,
    2, 0x84, 0x40,
    2, 0x85, 0xFF,
    2, 0x86, 0xFF,
    2, 0x87, 0xFF,
    2, 0x88, 0x0A,
    2, 0x89, 0x21,
    2, 0x8A, 0x00,
    2, 0x8B, 0x80,
    2, 0x8C, 0x01,
    2, 0x8D, 0x01,
    2, 0x8E, 0xFF,
    2, 0x8F, 0xFF,
    3, GC9A01_DISFUNCTRL, 0x00, 0x00,
    2, GC9A01_COLMOD, 0x55,
    5, 0x90, 0x08, 0x08, 0x08, 0x08,
    2, 0xBD, 0x06,
    2, 0xBC, 0x00,
    4, 0xFF, 0x60, 0x01, 0x04,
    2, GC9A01_PWCTR2, 0x13,
    2, GC9A01_PWCTR3, 0x13,
    2, GC9A01_PWCTR4, 0x22,
    2, 0xBE, 0x11,
    3, 0xE1, 0x10, 0x0E,
    4, 0xDF, 0x21, 0x0C, 0x02,
    7, GC9A01_SETGAMMA1, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A,
    7, GC9A01_SETGAMMA2, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
    7, GC9A01_SETGAMMA3, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A,
    7, GC9A01_SETGAMMA4, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
    3, 0xED, 0x1B, 0x0B,
    2, 0xAE, 0x77,
    2, 0xCD, 0x63,
    10, 0x70, 0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03,
    2, GC9A01_FRAMERATE, 0x34,
    13, 0x62, 0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70,
    13, 0x63, 0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70,
    8, 0x64, 0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07,
    11, 0x66, 0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00,
    11, 0x67, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98,
    8, 0x74, 0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00,
    3, 0x98, 0x3E, 0x07,
    1, GC9A01_TEON,
    1, GC9A01_INVON,
    1, GC9A01_SLPOUT,
    LCD_SEQUENCE_DELAY(100),
    2, GC9A01_MADCTL, 0x48,
    1, GC9A01_DISPON,
    LCD_SEQUENCE_DELAY(100),
LCD_SEQUENCE_END

/**
 * Initialize the GC9A01 display controller
 */
void
gc9a01_init(lv_disp_drv_t *driver)
{
    lcd_command_sequence(init_cmds);
}

static void
gc9a01_drv_update(struct _lv_disp_drv_t *drv)
{
    gc9a01_rotate(drv->rotated);
}

void
gc9a01_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint8_t cmd[5];

    if (area->x2 < 0 || area->y2 < 0 || area->x1 >= GC9A01_HOR_RES || area->y1 >= GC9A01_VER_RES) {
        lv_disp_flush_ready(drv);
        return;
    }

    /* Truncate the area to the screen */
    int32_t act_x1 = area->x1 < 0 ? 0 : area->x1;
    int32_t act_y1 = area->y1 < 0 ? 0 : area->y1;
    int32_t act_x2 = area->x2 >= GC9A01_HOR_RES ? GC9A01_HOR_RES - 1 : area->x2;
    int32_t act_y2 = area->y2 >= GC9A01_VER_RES ? GC9A01_VER_RES - 1 : area->y2;

    /* Column address */
    cmd[0] = GC9A01_CASET;
    cmd[1] = (uint8_t)(act_x1 >> 8);
    cmd[2] = (uint8_t)act_x1;
    cmd[3] = (uint8_t)(act_x2 >> 8);
    cmd[4] = (uint8_t)act_x2;
    lcd_ift_write_cmd(cmd, 5);

    /* Page address */
    cmd[0] = GC9A01_RASET;
    cmd[1] = (uint8_t)(act_y1 >> 8);
    cmd[2] = (uint8_t)act_y1;
    cmd[3] = (uint8_t)(act_y2 >> 8);
    cmd[4] = (uint8_t)act_y2;
    lcd_ift_write_cmd(cmd, 5);

    cmd[0] = GC9A01_RAMWR;
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
    driver->flush_cb = gc9a01_flush;
    driver->drv_update_cb = gc9a01_drv_update;
    driver->hor_res = GC9A01_TFTWIDTH;
    driver->ver_res = GC9A01_TFTHEIGHT;

    gc9a01_init(driver);
}
