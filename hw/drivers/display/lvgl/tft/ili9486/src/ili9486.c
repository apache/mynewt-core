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

#define ILI9486_TFTWIDTH    320
#define ILI9486_TFTHEIGHT   480

/* Level 1 Commands -------------- [section] Description */

#define ILI9486_NOP         0x00 /* [8.2.1 ] No Operation / Terminate Frame Memory Write */
#define ILI9486_SWRESET     0x01 /* [8.2.2 ] Software Reset */
#define ILI9486_RDDIDIF     0x04 /* [8.2.3 ] Read Display Identification Information */
#define ILI9486_RDNOE       0x05 /* [8.2.4 ] Read Number of the Errors on DSI */
#define ILI9486_RDDST       0x09 /* [8.2.5 ] Read Display Status */
#define ILI9486_RDDPM       0x0A /* [8.2.6 ] Read Display Power Mode */
#define ILI9486_RDDMADCTL   0x0B /* [8.2.7 ] Read Display MADCTL */
#define ILI9486_RDDCOLMOD   0x0C /* [8.2.8 ] Read Display Pixel Format */
#define ILI9486_RDDIM       0x0D /* [8.2.9 ] Read Display Image Mode */
#define ILI9486_RDDSM       0x0E /* [8.2.10] Read Display Signal Mode */
#define ILI9486_RDDSDR      0x0F /* [8.2.11] Read Display Self-Diagnostic Result */
#define ILI9486_SLPIN       0x10 /* [8.2.12] Enter Sleep Mode */
#define ILI9486_SLPOUT      0x11 /* [8.2.13] Leave Sleep Mode */
#define ILI9486_PTLON       0x12 /* [8.2.14] Partial Display Mode ON */
#define ILI9486_NORON       0x13 /* [8.2.15] Normal Display Mode ON */
#define ILI9486_DINVOFF     0x20 /* [8.2.16] Display Inversion OFF */
#define ILI9486_DINVON      0x21 /* [8.2.17] Display Inversion ON */
#define ILI9486_DISPOFF     0x28 /* [8.2.18] Display OFF */
#define ILI9486_DISPON      0x29 /* [8.2.19] Display ON */
#define ILI9486_CASET       0x2A /* [8.2.20] Column Address Set */
#define ILI9486_PASET       0x2B /* [8.2.21] Page Address Set */
#define ILI9486_RAMWR       0x2C /* [8.2.22] Memory Write */
#define ILI9486_RAMRD       0x2E /* [8.2.23] Memory Read */
#define ILI9486_PTLAR       0x30 /* [8.2.24] Partial Area */
#define ILI9486_VSCRDEF     0x33 /* [8.2.25] Vertical Scrolling Definition */
#define ILI9486_TEOFF       0x34 /* [8.2.26] Tearing Effect Line OFF */
#define ILI9486_TEON        0x35 /* [8.2.27] Tearing Effect Line ON */
#define ILI9486_MADCTL      0x36 /* [8.2.28] Memory Access Control */
#define     MADCTL_MY       0x80 /*          MY row address order */
#define     MADCTL_MX       0x40 /*          MX column address order */
#define     MADCTL_MV       0x20 /*          MV row / column exchange */
#define     MADCTL_ML       0x10 /*          ML vertical refresh order */
#define     MADCTL_MH       0x04 /*          MH horizontal refresh order */
#define     MADCTL_RGB      0x00 /*          RGB Order [default] */
#define     MADCTL_BGR      0x08 /*          BGR Order */
#define ILI9486_VSCRSADD    0x37 /* [8.2.29] Vertical Scrolling Start Address */
#define ILI9486_IDMOFF      0x38 /* [8.2.30] Idle Mode OFF */
#define ILI9486_IDMON       0x39 /* [8.2.31] Idle Mode ON */
#define ILI9486_PIXSET      0x3A /* [8.2.32] Pixel Format Set */
#define ILI9486_WRMEMCONT   0x3C /* [8.2.33] Write Memory Continue */
#define ILI9486_RDMEMCONT   0x3E /* [8.2.34] Read Memory Continue */
#define ILI9486_SETSCANTE   0x44 /* [8.2.35] Set Tear Scanline */
#define ILI9486_GETSCAN     0x45 /* [8.2.36] Get Scanline */
#define ILI9486_WRDISBV     0x51 /* [8.2.37] Write Display Brightness Value */
#define ILI9486_RDDISBV     0x52 /* [8.2.38] Read Display Brightness Value */
#define ILI9486_WRCTRLD     0x53 /* [8.2.39] Write Control Display */
#define ILI9486_RDCTRLD     0x54 /* [8.2.40] Read Control Display */
#define ILI9486_WRCABC      0x55 /* [8.2.41] Write Content Adaptive Brightness Control Value */
#define ILI9486_RDCABC      0x56 /* [8.2.42] Read Content Adaptive Brightness Control Value */
#define ILI9486_WRCABCMIN   0x5E /* [8.2.43] Write CABC Minimum Brightness */
#define ILI9486_RDCABCMIN   0x5F /* [8.2.44] Read CABC Minimum Brightness */
#define ILI9486_RDID1       0xDA /* [8.2.47] Read ID1 - Manufacturer ID (user) */
#define ILI9486_RDID2       0xDB /* [8.2.48] Read ID2 - Module/Driver version (supplier) */
#define ILI9486_RDID3       0xDC /* [8.2.49] Read ID3 - Module/Driver version (user) */

/* Level 2 Commands -------------- [section] Description */

#define ILI9486_IFMODE      0xB0 /* [8.2.50] Interface Mode Control */
#define ILI9486_FRMCTR1     0xB1 /* [8.2.51] Frame Rate Control (In Normal Mode/Full Colors) */
#define ILI9486_FRMCTR2     0xB2 /* [8.2.52] Frame Rate Control (In Idle Mode/8 colors) */
#define ILI9486_FRMCTR3     0xB3 /* [8.2.53] Frame Rate control (In Partial Mode/Full Colors) */
#define ILI9486_INVTR       0xB4 /* [8.2.54] Display Inversion Control */
#define ILI9486_PRCTR       0xB5 /* [8.2.55] Blanking Porch Control */
#define ILI9486_DISCTRL     0xB6 /* [8.2.56] Display Function Control */
#define ILI9486_ETMOD       0xB7 /* [8.2.57] Entry Mode Set */
#define ILI9486_PWCTRL1     0xC0 /* [8.2.58] Power Control 1 - GVDD */
#define ILI9486_PWCTRL2     0xC1 /* [8.2.59] Power Control 2 - step-up factor for operating voltage */
#define ILI9486_PWCTRL3     0xC2 /* [8.2.60] Power Control 3 - for normal mode */
#define ILI9486_PWCTRL4     0xC3 /* [8.2.61] Power Control 4 - for idle mode */
#define ILI9486_PWCTRL5     0xC4 /* [8.2.62] Power Control 5 - for partial mode */
#define ILI9486_VMCTRL      0xC5 /* [8.2.63] VCOM Control */
#define ILI9486_CABCCTRL1   0xC6 /* [8.2.64] VCOM Control */
#define ILI9486_CABCCTRL2   0xC8 /* [8.2.65] VCOM Control */
#define ILI9486_CABCCTRL3   0xC9 /* [8.2.66] VCOM Control */
#define ILI9486_CABCCTRL4   0xCA /* [8.2.67] VCOM Control */
#define ILI9486_CABCCTRL5   0xCB /* [8.2.68] VCOM Control */
#define ILI9486_CABCCTRL6   0xCC /* [8.2.69] VCOM Control */
#define ILI9486_CABCCTRL7   0xCD /* [8.2.70] VCOM Control */
#define ILI9486_CABCCTRL8   0xCE /* [8.2.71] VCOM Control */
#define ILI9486_CABCCTRL9   0xCF /* [8.2.72] VCOM Control */
#define ILI9486_NVMWR       0xD0 /* [8.2.73] NV Memory Write */
#define ILI9486_NVMPKEY     0xD1 /* [8.2.74] NV Memory Protection Key */
#define ILI9486_RDNVM       0xD2 /* [8.2.75] NV Memory Status Read */
#define ILI9486_RDID4       0xD3 /* [8.2.76] Read ID4 - IC Device Code */
#define ILI9486_PGAMCTRL    0xE0 /* [8.2.77] Positive Gamma Control */
#define ILI9486_NGAMCTRL    0xE1 /* [8.2.78] Negative Gamma Correction */
#define ILI9486_DGAMCTRL1   0xE2 /* [8.2.79] Digital Gamma Control 1 */
#define ILI9486_DGAMCTRL2   0xE3 /* [8.2.80] Digital Gamma Control 2 */
#define ILI9486_SPIRCS      0xFB /* [8.2.81] SPI read command settings */

#define ILI9486_HOR_RES       ILI9486_TFTWIDTH
#define ILI9486_VER_RES       ILI9486_TFTHEIGHT

static const uint8_t madctl[] = {
    0,
    MADCTL_MV | MADCTL_MY | MADCTL_ML,
    MADCTL_MX | MADCTL_MY,
    MADCTL_MX | MADCTL_MV,
};

void
ili9486_rotate(lv_disp_rot_t rotation)
{
    uint8_t madcmd[2] = { ILI9486_MADCTL, MADCTL_BGR | madctl[rotation] };

    lcd_ift_write_cmd(madcmd, 2);
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
        1, ILI9486_NOP,
#if MYNEWT_VAL(LCD_RESET_PIN) < 0
        1, ILI9486_SWRESET,
        LCD_SEQUENCE_DELAY(5),
#endif
        3, ILI9486_PWCTRL1, 0x19, 0x1A,
        3, ILI9486_PWCTRL2, 0x45, 0x00,
        2, ILI9486_PWCTRL3, 0x33,
        3, ILI9486_VMCTRL, 0x00, 0x28,
        3, ILI9486_FRMCTR1, 0xA0, 0x11,
        2 ,ILI9486_INVTR, 0x02,
        4, ILI9486_DISCTRL, 0x00, 0x42, 0x3B,
        /* positive gamma correction */
        16, ILI9486_PGAMCTRL, 0x0F, 0x1F, 0x1C, 0x0C, 0x0F, 0x08, 0x48, 0x98,
            0x37, 0x0A, 0x13, 0x04, 0x11, 0x0D, 0x00,
        16, ILI9486_NGAMCTRL, 0x0F, 0x32, 0x2E, 0x0B, 0x0D, 0x05, 0x47, 0x75,
            0x37, 0x06, 0x10, 0x03, 0x24, 0x20, 0x00,
        /* 16 bit pixel */
        2, ILI9486_PIXSET, 0x55,
        3, ILI9486_DISCTRL, 0x00, 0x22,
        2, ILI9486_MADCTL, MADCTL_BGR,
        1, ILI9486_SLPOUT,
        LCD_SEQUENCE_DELAY(100),
        1, ILI9486_DISPON,
LCD_SEQUENCE_END

/**
 * Initialize the ILI9486 display controller
 */
void
ili9486_init(lv_disp_drv_t *driver)
{
    lcd_command_sequence(init_cmds);
}

static void
ili9486_drv_update(struct _lv_disp_drv_t *drv)
{
    ili9486_rotate(drv->rotated);
}

void
ili9486_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
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

    /* Column address */
    cmd[0] = ILI9486_CASET;
    cmd[1] = (uint8_t)(act_x1 >> 8);
    cmd[2] = (uint8_t)act_x1;
    cmd[3] = (uint8_t)(act_x2 >> 8);
    cmd[4] = (uint8_t)act_x2;
    lcd_ift_write_cmd(cmd, 5);

    /* Page address */
    cmd[0] = ILI9486_PASET;
    cmd[1] = (uint8_t)(act_y1 >> 8);
    cmd[2] = (uint8_t)act_y1;
    cmd[3] = (uint8_t)(act_y2 >> 8);
    cmd[4] = (uint8_t)act_y2;
    lcd_ift_write_cmd(cmd, 5);

    cmd[0] = ILI9486_RAMWR;
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
    driver->flush_cb = ili9486_flush;
    driver->drv_update_cb = ili9486_drv_update;
    driver->hor_res = ILI9486_TFTWIDTH;
    driver->ver_res = ILI9486_TFTHEIGHT;

    ili9486_init(driver);
}
