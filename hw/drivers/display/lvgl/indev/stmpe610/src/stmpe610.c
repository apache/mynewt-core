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

#include <os/os_time.h>
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <bus/drivers/spi_common.h>

#include <hal/lv_hal_indev.h>
#include <core/lv_disp.h>

#define STMPE610_XY_SWAP         MYNEWT_VAL(STMPE610_XY_SWAP)
#define STMPE610_X_INV           MYNEWT_VAL(STMPE610_X_INV)
#define STMPE610_Y_INV           MYNEWT_VAL(STMPE610_Y_INV)
#define STMPE610_MIN_X           MYNEWT_VAL(STMPE610_MIN_X)
#define STMPE610_MIN_Y           MYNEWT_VAL(STMPE610_MIN_Y)
#define STMPE610_MAX_X           MYNEWT_VAL(STMPE610_MAX_X)
#define STMPE610_MAX_Y           MYNEWT_VAL(STMPE610_MAX_Y)
#define STMPE610_X_RANGE         ((STMPE610_MAX_X) - (STMPE610_MIN_X))
#define STMPE610_Y_RANGE         ((STMPE610_MAX_Y) - (STMPE610_MIN_Y))
#define STMPE610_HOR_RES         MYNEWT_VAL_LVGL_DISPLAY_HORIZONTAL_RESOLUTION
#define STMPE610_VER_RES         MYNEWT_VAL_LVGL_DISPLAY_VERTICAL_RESOLUTION

struct bus_spi_node touch;
struct bus_spi_node_cfg touch_spi_cfg = {
    .node_cfg.bus_name = MYNEWT_VAL(STMPE610_SPI_DEV_NAME),
    .pin_cs = MYNEWT_VAL(STMPE610_SPI_CS_PIN),
    .mode = BUS_SPI_MODE_0,
    .data_order = HAL_SPI_MSB_FIRST,
    .freq = MYNEWT_VAL(STMPE610_SPI_FREQ),
};

static struct os_dev *touch_dev;
static lv_indev_drv_t stmpe610_drv;
static lv_indev_t *stmpe610_dev;

/** Reset Control **/
#define STMPE_SYS_CTRL1             0x03
#define STMPE_SYS_CTRL1_RESET       0x02

#define STMPE_SYS_CTRL2             0x04

#define STMPE_SPI_CFG               0x08
#define STMPE_SPI_AUTO_INCR         0x04

#define STMPE_TSC_CTRL              0x40
#define STMPE_TSC_CTRL_EN           0x01
#define STMPE_TSC_CTRL_XYZ          0x00
#define STMPE_TSC_CTRL_XY           0x02
#define STMPE_TSC_CTRL_X            0x04
#define STMPE_TSC_CTRL_Y            0x06
#define STMPE_TSC_CTRL_Z            0x08
#define STMPE_TSC_CTRL_TSC_STA      0x80

/** Interrupt control **/
#define STMPE_INT_CTRL              0x09
#define STMPE_INT_CTRL_POL_HIGH     0x04
#define STMPE_INT_CTRL_POL_LOW      0x00
#define STMPE_INT_CTRL_EDGE         0x02
#define STMPE_INT_CTRL_LEVEL        0x00
#define STMPE_INT_CTRL_ENABLE       0x01
#define STMPE_INT_CTRL_DISABLE      0x00

/** Interrupt enable **/
#define STMPE_INT_EN                0x0A
#define STMPE_INT_EN_TOUCHDET       0x01
#define STMPE_INT_EN_FIFOTH         0x02
#define STMPE_INT_EN_FIFOOF         0x04
#define STMPE_INT_EN_FIFOFULL       0x08
#define STMPE_INT_EN_FIFOEMPTY      0x10
#define STMPE_INT_EN_ADC            0x40
#define STMPE_INT_EN_GPIO           0x80

/** Interrupt status **/
#define STMPE_INT_STA               0x0B
#define STMPE_INT_STA_TOUCHDET      0x01
#define STMPE_INT_STA_FIFO_THT      0x02

/** ADC control **/
#define STMPE_ADC_CTRL1             0x20
#define STMPE_ADC_CTRL1_12BIT       0x08
#define STMPE_ADC_CTRL1_10BIT       0x00

/** ADC control **/
#define STMPE_ADC_CTRL2             0x21
#define STMPE_ADC_CTRL2_1_625MHZ    0x00
#define STMPE_ADC_CTRL2_3_25MHZ     0x01
#define STMPE_ADC_CTRL2_6_5MHZ      0x02

/** Touchscreen controller configuration **/
#define STMPE_TSC_CFG               0x41
#define STMPE_TSC_CFG_1SAMPLE       0x00
#define STMPE_TSC_CFG_2SAMPLE       0x40
#define STMPE_TSC_CFG_4SAMPLE       0x80
#define STMPE_TSC_CFG_8SAMPLE       0xC0
#define STMPE_TSC_CFG_DELAY_10US    0x00
#define STMPE_TSC_CFG_DELAY_50US    0x08
#define STMPE_TSC_CFG_DELAY_100US   0x10
#define STMPE_TSC_CFG_DELAY_500US   0x18
#define STMPE_TSC_CFG_DELAY_1MS     0x20
#define STMPE_TSC_CFG_DELAY_5MS     0x28
#define STMPE_TSC_CFG_DELAY_10MS    0x30
#define STMPE_TSC_CFG_DELAY_50MS    0x38
#define STMPE_TSC_CFG_SETTLE_10US   0x00
#define STMPE_TSC_CFG_SETTLE_100US  0x01
#define STMPE_TSC_CFG_SETTLE_500US  0x02
#define STMPE_TSC_CFG_SETTLE_1MS    0x03
#define STMPE_TSC_CFG_SETTLE_5MS    0x04
#define STMPE_TSC_CFG_SETTLE_10MS   0x05
#define STMPE_TSC_CFG_SETTLE_50MS   0x06
#define STMPE_TSC_CFG_SETTLE_100MS  0x07

/** FIFO level to generate interrupt **/
#define STMPE_FIFO_TH               0x4A

/** Current filled level of FIFO **/
#define STMPE_FIFO_SIZE             0x4C

/** Current status of FIFO **/
#define STMPE_FIFO_STA              0x4B
#define STMPE_FIFO_STA_RESET        0x01
#define STMPE_FIFO_STA_OFLOW        0x80
#define STMPE_FIFO_STA_FULL         0x40
#define STMPE_FIFO_STA_EMPTY        0x20
#define STMPE_FIFO_STA_THTRIG       0x10

/** Touchscreen controller drive I **/
#define STMPE_TSC_I_DRIVE           0x58
#define STMPE_TSC_I_DRIVE_20MA      0x00
#define STMPE_TSC_I_DRIVE_50MA      0x01

/** Data port for TSC data address **/
#define STMPE_TSC_DATA_X            0x4D
#define STMPE_TSC_DATA_Y            0x4F
#define STMPE_TSC_FRACTION_Z        0x56
#define STMPE_TSC_DATA              0x57

/** GPIO **/
#define STMPE_GPIO_SET_PIN          0x10
#define STMPE_GPIO_CLR_PIN          0x11
#define STMPE_GPIO_DIR              0x13
#define STMPE_GPIO_ALT_FUNCT        0x17


struct touch_screen_data {
    /* ADC value for left edge */
    uint16_t left;
    /* ADC value for right edge */
    uint16_t right;
    /* ADC value for top edge */
    uint16_t top;
    /* ADC value for bottom edge */
    uint16_t bottom;
    /* Current value for X, Y detected */
    int x;
    int y;
    /* Values that were last reported */
    int last_x;
    int last_y;
};

static struct touch_screen_data touch_screen_data = {
    .left = MYNEWT_VAL(STMPE610_MIN_X),
    .right = MYNEWT_VAL(STMPE610_MAX_X),
    .top = MYNEWT_VAL(STMPE610_MIN_Y),
    .bottom = MYNEWT_VAL(STMPE610_MAX_Y),
};

static void
xpt2046_corr(int16_t *xp, int16_t *yp)
{
    int x;
    int y;
#if STMPE610_XY_SWAP
    x = *yp;
    y = *xp;
#else
    x = *xp;
    y = *yp;
#endif

    if (x < MYNEWT_VAL(STMPE610_MIN_X)) {
        x = MYNEWT_VAL(STMPE610_MIN_X);
    }
    if (y < MYNEWT_VAL(STMPE610_MIN_Y)) {
        y = MYNEWT_VAL(STMPE610_MIN_Y);
    }

    x = (x - STMPE610_MIN_X) * STMPE610_HOR_RES / STMPE610_X_RANGE;
    y = (y - STMPE610_MIN_Y) * STMPE610_VER_RES / STMPE610_Y_RANGE;

#if STMPE610_X_INV
    x = STMPE610_HOR_RES - x;
#endif

#if STMPE610_Y_INV
    y = STMPE610_VER_RES - y;
#endif
    *xp = x;
    *yp = y;
}

static void
stmpe610_spi_write_then_read(const uint8_t *wbuf, int wlen, uint8_t *rbuf, int rlen)
{
    if (rlen) {
        bus_node_simple_write_read_transact(touch_dev, wbuf, wlen, rbuf, rlen);
    } else {
        bus_node_simple_write(touch_dev, wbuf, wlen);
    }
}

static void
stmpe610_write_reg8(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    stmpe610_spi_write_then_read(buf, 2, NULL, 0);
}

static uint8_t
stmpe610_read_reg8(uint8_t reg)
{
    uint8_t val;
    reg |= 0x80;

    stmpe610_spi_write_then_read(&reg, 1, &val, 1);

    return val;
}

/**
 * Get the current position and state of the touchpad
 * @param data store the read data here
 */
static void
stmpe610_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    /* If there is not INT pin configured, assume int is active and read registers */
    bool int_detected = MYNEWT_VAL(STMPE610_INT_PIN) < 0 || hal_gpio_read(MYNEWT_VAL(STMPE610_INT_PIN));
    bool touch_detected = false;
    uint8_t int_sta = 0;
    uint8_t buf[4];

    int16_t x = touch_screen_data.last_x;
    int16_t y = touch_screen_data.last_y;

    if (int_detected) {
        int_sta = stmpe610_read_reg8(STMPE_INT_STA);
        /* Get only the last value from FIFO */
        buf[0] = STMPE_TSC_DATA | 0x80;
        while ((stmpe610_read_reg8(STMPE_FIFO_STA) & STMPE_FIFO_STA_EMPTY) == 0) {
            stmpe610_spi_write_then_read(buf, 1, buf + 1, 1);
            stmpe610_spi_write_then_read(buf, 1, buf + 2, 1);
            stmpe610_spi_write_then_read(buf, 1, buf + 3, 1);
            touch_detected = true;
        }

        if (touch_detected) {
            x = buf[1] << 4 | (buf[2] >> 4);
            y = ((buf[2] & 0xF) << 8) | buf[3];

            xpt2046_corr(&x, &y);

            touch_screen_data.last_x = x;
            touch_screen_data.last_y = y;
        }
    }
    if (int_sta) {
        /* Clear raised interrupts */
        stmpe610_write_reg8(STMPE_INT_STA, int_sta);
    }

    data->state = touch_detected ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->point.x = x;
    data->point.y = y;
}

/**
 * Initialize the STMPE610
 */
void
stmpe610_register_lv_indev(void)
{
    stmpe610_read_reg8(0);
    stmpe610_read_reg8(1);
    stmpe610_read_reg8(STMPE_SPI_CFG);
    stmpe610_write_reg8(STMPE_SYS_CTRL1, STMPE_SYS_CTRL1_RESET);
    os_time_delay(os_time_ticks_to_ms32(10));
    /* Clocks on */
    stmpe610_write_reg8(STMPE_SYS_CTRL2, 0x0);
    stmpe610_write_reg8(STMPE_SPI_CFG, STMPE_SPI_AUTO_INCR);
    stmpe610_read_reg8(STMPE_SPI_CFG);
    /* Enable XY */
    stmpe610_write_reg8(STMPE_TSC_CTRL,
                   STMPE_TSC_CTRL_XY | STMPE_TSC_CTRL_EN);
    stmpe610_write_reg8(STMPE_INT_EN, STMPE_INT_EN_TOUCHDET | STMPE_INT_EN_FIFOTH);
    /* 96 clocks per conversion */
    stmpe610_write_reg8(STMPE_ADC_CTRL1, STMPE_ADC_CTRL1_10BIT |
                        (0x6 << 4));
    stmpe610_write_reg8(STMPE_ADC_CTRL2, STMPE_ADC_CTRL2_6_5MHZ);
    stmpe610_write_reg8(STMPE_TSC_CFG, STMPE_TSC_CFG_4SAMPLE |
                                       STMPE_TSC_CFG_DELAY_1MS |
                                       STMPE_TSC_CFG_SETTLE_5MS);
    stmpe610_write_reg8(STMPE_TSC_FRACTION_Z, 0x6);
    stmpe610_write_reg8(STMPE_FIFO_TH, 1);
    /* Reset */
    stmpe610_write_reg8(STMPE_FIFO_STA, STMPE_FIFO_STA_RESET);
    stmpe610_write_reg8(STMPE_FIFO_STA, 0);
    stmpe610_write_reg8(STMPE_TSC_I_DRIVE, STMPE_TSC_I_DRIVE_50MA);
    /* reset all ints */
    stmpe610_write_reg8(STMPE_INT_STA, 0xFF);
    stmpe610_write_reg8(STMPE_INT_CTRL,
                        STMPE_INT_CTRL_POL_HIGH | STMPE_INT_CTRL_ENABLE);

    /* Register a keypad input device */
    lv_indev_drv_init(&stmpe610_drv);
    stmpe610_drv.type = LV_INDEV_TYPE_POINTER;
    stmpe610_drv.read_cb = stmpe610_read;
    stmpe610_dev = lv_indev_drv_register(&stmpe610_drv);
}

void
stmpe610_os_dev_create(void)
{
    struct bus_node_callbacks cbs = { 0 };
    int rc;

    if (MYNEWT_VAL(STMPE610_INT_PIN) >= 0) {
        hal_gpio_init_in(MYNEWT_VAL(STMPE610_INT_PIN), HAL_GPIO_PULL_NONE);
    }
    hal_gpio_init_out(MYNEWT_VAL(STMPE610_SPI_CS_PIN), 1);

    bus_node_set_callbacks((struct os_dev *)&touch, &cbs);

    rc = bus_spi_node_create("touch", &touch, &touch_spi_cfg, NULL);
    assert(rc == 0);
    touch_dev = os_dev_open("touch", 0, NULL);
}
