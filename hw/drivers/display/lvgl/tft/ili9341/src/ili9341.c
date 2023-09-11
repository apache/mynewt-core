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

#include <syscfg/syscfg.h>
#include <bsp/bsp.h>
#include <lv_glue.h>
#include <hal/lv_hal_disp.h>
#include <hal/hal_gpio.h>
#include <lcd_itf.h>

#define ILI9341_TFTWIDTH    240
#define ILI9341_TFTHEIGHT   320

/* Level 1 Commands -------------- [section] Description */

#define ILI9341_NOP         0x00 /* [8.2.1 ] No Operation / Terminate Frame Memory Write */
#define ILI9341_SWRESET     0x01 /* [8.2.2 ] Software Reset */
#define ILI9341_RDDIDIF     0x04 /* [8.2.3 ] Read Display Identification Information */
#define ILI9341_RDDST       0x09 /* [8.2.4 ] Read Display Status */
#define ILI9341_RDDPM       0x0A /* [8.2.5 ] Read Display Power Mode */
#define ILI9341_RDDMADCTL   0x0B /* [8.2.6 ] Read Display MADCTL */
#define ILI9341_RDDCOLMOD   0x0C /* [8.2.7 ] Read Display Pixel Format */
#define ILI9341_RDDIM       0x0D /* [8.2.8 ] Read Display Image Mode */
#define ILI9341_RDDSM       0x0E /* [8.2.9 ] Read Display Signal Mode */
#define ILI9341_RDDSDR      0x0F /* [8.2.10] Read Display Self-Diagnostic Result */
#define ILI9341_SLPIN       0x10 /* [8.2.11] Enter Sleep Mode */
#define ILI9341_SLPOUT      0x11 /* [8.2.12] Leave Sleep Mode */
#define ILI9341_PTLON       0x12 /* [8.2.13] Partial Display Mode ON */
#define ILI9341_NORON       0x13 /* [8.2.14] Normal Display Mode ON */
#define ILI9341_DINVOFF     0x20 /* [8.2.15] Display Inversion OFF */
#define ILI9341_DINVON      0x21 /* [8.2.16] Display Inversion ON */
#define ILI9341_GAMSET      0x26 /* [8.2.17] Gamma Set */
#define ILI9341_DISPOFF     0x28 /* [8.2.18] Display OFF*/
#define ILI9341_DISPON      0x29 /* [8.2.19] Display ON*/
#define ILI9341_CASET       0x2A /* [8.2.20] Column Address Set */
#define ILI9341_PASET       0x2B /* [8.2.21] Page Address Set */
#define ILI9341_RAMWR       0x2C /* [8.2.22] Memory Write */
#define ILI9341_RGBSET      0x2D /* [8.2.23] Color Set (LUT for 16-bit to 18-bit color depth conversion) */
#define ILI9341_RAMRD       0x2E /* [8.2.24] Memory Read */
#define ILI9341_PTLAR       0x30 /* [8.2.25] Partial Area */
#define ILI9341_VSCRDEF     0x33 /* [8.2.26] Veritcal Scrolling Definition */
#define ILI9341_TEOFF       0x34 /* [8.2.27] Tearing Effect Line OFF */
#define ILI9341_TEON        0x35 /* [8.2.28] Tearing Effect Line ON */
#define ILI9341_MADCTL      0x36 /* [8.2.29] Memory Access Control */
#define     MADCTL_MY       0x80 /*          MY row address order */
#define     MADCTL_MX       0x40 /*          MX column address order */
#define     MADCTL_MV       0x20 /*          MV row / column exchange */
#define     MADCTL_ML       0x10 /*          ML vertical refresh order */
#define     MADCTL_MH       0x04 /*          MH horizontal refresh order */
#define     MADCTL_RGB      0x00 /*          RGB Order [default] */
#define     MADCTL_BGR      0x08 /*          BGR Order */
#define ILI9341_VSCRSADD    0x37 /* [8.2.30] Vertical Scrolling Start Address */
#define ILI9341_IDMOFF      0x38 /* [8.2.31] Idle Mode OFF */
#define ILI9341_IDMON       0x39 /* [8.2.32] Idle Mode ON */
#define ILI9341_PIXSET      0x3A /* [8.2.33] Pixel Format Set */
#define ILI9341_WRMEMCONT   0x3C /* [8.2.34] Write Memory Continue */
#define ILI9341_RDMEMCONT   0x3E /* [8.2.35] Read Memory Continue */
#define ILI9341_SETSCANTE   0x44 /* [8.2.36] Set Tear Scanline */
#define ILI9341_GETSCAN     0x45 /* [8.2.37] Get Scanline */
#define ILI9341_WRDISBV     0x51 /* [8.2.38] Write Display Brightness Value */
#define ILI9341_RDDISBV     0x52 /* [8.2.39] Read Display Brightness Value */
#define ILI9341_WRCTRLD     0x53 /* [8.2.40] Write Control Display */
#define ILI9341_RDCTRLD     0x54 /* [8.2.41] Read Control Display */
#define ILI9341_WRCABC      0x55 /* [8.2.42] Write Content Adaptive Brightness Control Value */
#define ILI9341_RDCABC      0x56 /* [8.2.43] Read Content Adaptive Brightness Control Value */
#define ILI9341_WRCABCMIN   0x5E /* [8.2.44] Write CABC Minimum Brightness */
#define ILI9341_RDCABCMIN   0x5F /* [8.2.45] Read CABC Minimum Brightness */
#define ILI9341_RDID1       0xDA /* [8.2.46] Read ID1 - Manufacturer ID (user) */
#define ILI9341_RDID2       0xDB /* [8.2.47] Read ID2 - Module/Driver version (supplier) */
#define ILI9341_RDID3       0xDC /* [8.2.48] Read ID3 - Module/Driver version (user) */

/* Level 2 Commands -------------- [section] Description */

#define ILI9341_IFMODE      0xB0 /* [8.3.1 ] Interface Mode Control */
#define ILI9341_FRMCTR1     0xB1 /* [8.3.2 ] Frame Rate Control (In Normal Mode/Full Colors) */
#define ILI9341_FRMCTR2     0xB2 /* [8.3.3 ] Frame Rate Control (In Idle Mode/8 colors) */
#define ILI9341_FRMCTR3     0xB3 /* [8.3.4 ] Frame Rate control (In Partial Mode/Full Colors) */
#define ILI9341_INVTR       0xB4 /* [8.3.5 ] Display Inversion Control */
#define ILI9341_PRCTR       0xB5 /* [8.3.6 ] Blanking Porch Control */
#define ILI9341_DISCTRL     0xB6 /* [8.3.7 ] Display Function Control */
#define ILI9341_ETMOD       0xB7 /* [8.3.8 ] Entry Mode Set */
#define ILI9341_BLCTRL1     0xB8 /* [8.3.9 ] Backlight Control 1 - Grayscale Histogram UI mode */
#define ILI9341_BLCTRL2     0xB9 /* [8.3.10] Backlight Control 2 - Grayscale Histogram still picture mode */
#define ILI9341_BLCTRL3     0xBA /* [8.3.11] Backlight Control 3 - Grayscale Thresholds UI mode */
#define ILI9341_BLCTRL4     0xBB /* [8.3.12] Backlight Control 4 - Grayscale Thresholds still picture mode */
#define ILI9341_BLCTRL5     0xBC /* [8.3.13] Backlight Control 5 - Brightness Transition time */
#define ILI9341_BLCTRL7     0xBE /* [8.3.14] Backlight Control 7 - PWM Frequency */
#define ILI9341_BLCTRL8     0xBF /* [8.3.15] Backlight Control 8 - ON/OFF + PWM Polarity*/
#define ILI9341_PWCTRL1     0xC0 /* [8.3.16] Power Control 1 - GVDD */
#define ILI9341_PWCTRL2     0xC1 /* [8.3.17] Power Control 2 - step-up factor for operating voltage */
#define ILI9341_VMCTRL1     0xC5 /* [8.3.18] VCOM Control 1 - Set VCOMH and VCOML */
#define ILI9341_VMCTRL2     0xC7 /* [8.3.19] VCOM Control 2 - VCOM offset voltage */
#define ILI9341_NVMWR       0xD0 /* [8.3.20] NV Memory Write */
#define ILI9341_NVMPKEY     0xD1 /* [8.3.21] NV Memory Protection Key */
#define ILI9341_RDNVM       0xD2 /* [8.3.22] NV Memory Status Read */
#define ILI9341_RDID4       0xD3 /* [8.3.23] Read ID4 - IC Device Code */
#define ILI9341_PGAMCTRL    0xE0 /* [8.3.24] Positive Gamma Control */
#define ILI9341_NGAMCTRL    0xE1 /* [8.3.25] Negative Gamma Correction */
#define ILI9341_DGAMCTRL1   0xE2 /* [8.3.26] Digital Gamma Control 1 */
#define ILI9341_DGAMCTRL2   0xE3 /* [8.3.27] Digital Gamma Control 2 */
#define ILI9341_IFCTL       0xF6 /* [8.3.28] 16bits Data Format Selection */

/* Extended Commands --------------- [section] Description*/

#define ILI9341_PWCTRLA       0xCB /* [8.4.1] Power control A */
#define ILI9341_PWCTRLB       0xCF /* [8.4.2] Power control B */
#define ILI9341_TIMECTRLA_INT 0xE8 /* [8.4.3] Internal Clock Driver timing control A */
#define ILI9341_TIMECTRLA_EXT 0xE9 /* [8.4.4] External Clock Driver timing control A */
#define ILI9341_TIMECTRLB     0xEA /* [8.4.5] Driver timing control B (gate driver timing control) */
#define ILI9341_PWSEQCTRL     0xED /* [8.4.6] Power on sequence control */
#define ILI9341_GAM3CTRL      0xF2 /* [8.4.7] Enable 3 gamma control */
#define ILI9341_PUMPRATIO     0xF7 /* [8.4.8] Pump ratio control */

#define ILI9341_HOR_RES       ILI9341_TFTWIDTH
#define ILI9341_VER_RES       ILI9341_TFTHEIGHT

static const uint8_t madctl[] = {
    MADCTL_MX,
    MADCTL_MX | MADCTL_MY | MADCTL_MV,
    MADCTL_MY,
    MADCTL_MV,
};

void
ili9341_rotate(lv_disp_rot_t rotation)
{
    uint8_t madcmd[2] = { ILI9341_MADCTL, MADCTL_BGR | madctl[rotation] };

    lcd_ift_write_cmd(madcmd, 2);
}

LCD_SEQUENCE(init_cmds)
        LCD_SEQUENCE_LCD_CS_INACTIVATE(),
        LCD_SEQUENCE_LCD_DC_DATA(),
#if MYNEWT_VAL(LCD_RESET_PIN) >= 0
        LCD_SEQUENCE_LCD_RESET_ACTIVATE(),
        LCD_SEQUENCE_DELAY_US(50),
        LCD_SEQUENCE_LCD_RESET_INACTIVATE(),
        LCD_SEQUENCE_DELAY(5),
#endif
        1, ILI9341_NOP,
#if MYNEWT_VAL(LCD_RESET_PIN) < 0
        1, ILI9341_SWRESET,
        LCD_SEQUENCE_DELAY(5),
#endif
        1, ILI9341_DISPOFF,
        4, ILI9341_PWCTRLB, 0x00, 0x83, 0x30,
        5, ILI9341_PWSEQCTRL, 0x64, 0x03, 0x12, 0x81,
        4, ILI9341_TIMECTRLA_INT, 0x85, 0x01, 0x79,
        6, ILI9341_PWCTRLA, 0x39, 0x2c, 0x00, 0x34, 0x02,
        2, ILI9341_PUMPRATIO, 0x20,
        3, ILI9341_TIMECTRLB, 0x00, 0x00,
        2, ILI9341_PWCTRL1, 0x26,
        2, ILI9341_PWCTRL2, 0x11,
        3, ILI9341_VMCTRL1, 0x35, 0x3E,
        2, ILI9341_VMCTRL2, 0xBE,
        2, ILI9341_MADCTL, MADCTL_BGR | MADCTL_MX,
        2 ,ILI9341_PIXSET, 0x55,
        3, ILI9341_FRMCTR1, 0x00, 0x1B,
        /* positive gamma correction */
        2, ILI9341_GAMSET, 01,
        16, ILI9341_PGAMCTRL, 0x1f, 0x1a, 0x18, 0x0a, 0x0f, 0x06, 0x45, 0x87, 0x32, 0x0a, 0x07, 0x02, 0x07, 0x05, 0x00,
        16, ILI9341_NGAMCTRL, 0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3a, 0x78, 0x4d, 0x05, 0x18, 0x0d, 0x38, 0x3a, 0x1f,
        5, ILI9341_CASET, 0x00, 0x00, 0x00, 0xEF,
        5, ILI9341_PASET, 0x00, 0x00, 0x01, 0x3F,
        1, ILI9341_TEON,
        2, ILI9341_ETMOD, 0x07,
        5, ILI9341_DISCTRL, 0x0a, 0x82, 0x27, 0x00,
        1, ILI9341_SLPOUT,
        LCD_SEQUENCE_DELAY(100),
        1, ILI9341_DISPON,
        LCD_SEQUENCE_DELAY(20),
LCD_SEQUENCE_END

/**
 * Initialize the ILI9341 display controller
 */
void
ili9341_init(lv_disp_drv_t *driver)
{
    lcd_command_sequence(init_cmds);
}

static void
ili9341_drv_update(struct _lv_disp_drv_t *drv)
{
    ili9341_rotate(drv->rotated);
}

void
ili9341_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint8_t cmd[5];

    if (area->x2 < 0 || area->y2 < 0 || area->x1 >= ILI9341_HOR_RES || area->y1 >= ILI9341_VER_RES) {
        lv_disp_flush_ready(drv);
        return;
    }

    /* Truncate the area to the screen */
    int32_t act_x1 = area->x1 < 0 ? 0 : area->x1;
    int32_t act_y1 = area->y1 < 0 ? 0 : area->y1;
    int32_t act_x2 = area->x2 >= ILI9341_HOR_RES ? ILI9341_HOR_RES - 1 : area->x2;
    int32_t act_y2 = area->y2 >= ILI9341_VER_RES ? ILI9341_VER_RES - 1 : area->y2;

    /* Column address */
    cmd[0] = ILI9341_CASET;
    cmd[1] = (uint8_t)(act_x1 >> 8);
    cmd[2] = (uint8_t)act_x1;
    cmd[3] = (uint8_t)(act_x2 >> 8);
    cmd[4] = (uint8_t)act_x2;
    lcd_ift_write_cmd(cmd, 5);

    /* Page address */
    cmd[0] = ILI9341_PASET;
    cmd[1] = (uint8_t)(act_y1 >> 8);
    cmd[2] = (uint8_t)act_y1;
    cmd[3] = (uint8_t)(act_y2 >> 8);
    cmd[4] = (uint8_t)act_y2;
    lcd_ift_write_cmd(cmd, 5);

    cmd[0] = ILI9341_RAMWR;
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
    driver->flush_cb = ili9341_flush;
    driver->drv_update_cb = ili9341_drv_update;
    driver->hor_res = ILI9341_TFTWIDTH;
    driver->ver_res = ILI9341_TFTHEIGHT;

    ili9341_init(driver);
}
