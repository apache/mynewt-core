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


#include <stddef.h>
#include <stdio.h>
#include <os/os_time.h>
#include <hal/hal_gpio.h>
#include <bus/drivers/spi_common.h>
#include <bsp/bsp.h>

#include <hal/lv_hal_indev.h>
#include <core/lv_disp.h>

#define CMD_X_READ              0x90
#define CMD_Y_READ              0xD0
#define XPT2046_XY_SWAP         MYNEWT_VAL(XPT2046_XY_SWAP)
#define XPT2046_X_INV           MYNEWT_VAL(XPT2046_X_INV)
#define XPT2046_Y_INV           MYNEWT_VAL(XPT2046_Y_INV)
#define XPT2046_MIN_X           MYNEWT_VAL(XPT2046_MIN_X)
#define XPT2046_MIN_Y           MYNEWT_VAL(XPT2046_MIN_Y)
#define XPT2046_MAX_X           MYNEWT_VAL(XPT2046_MAX_X)
#define XPT2046_MAX_Y           MYNEWT_VAL(XPT2046_MAX_Y)
#define XPT2046_X_RANGE         ((XPT2046_MAX_X) - (XPT2046_MIN_X))
#define XPT2046_Y_RANGE         ((XPT2046_MAX_Y) - (XPT2046_MIN_Y))
#define XPT2046_HOR_RES         MYNEWT_VAL(XPT2046_HOR_RES)
#define XPT2046_VER_RES         MYNEWT_VAL(XPT2046_VER_RES)

struct bus_spi_node touch;
struct bus_spi_node_cfg touch_spi_cfg = {
    .node_cfg.bus_name = MYNEWT_VAL(XPT2046_SPI_DEV_NAME),
    .pin_cs = MYNEWT_VAL(XPT2046_SPI_CS_PIN),
    .mode = BUS_SPI_MODE_0,
    .data_order = HAL_SPI_MSB_FIRST,
    .freq = MYNEWT_VAL(XPT2046_SPI_FREQ),
};

static struct os_dev *touch_dev;
static lv_indev_drv_t xpt2046_drv;
static lv_indev_t *xpt2046_dev;

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
    .left = MYNEWT_VAL(XPT2046_MIN_X),
    .right = MYNEWT_VAL(XPT2046_MAX_X),
    .top = MYNEWT_VAL(XPT2046_MIN_Y),
    .bottom = MYNEWT_VAL(XPT2046_MAX_Y),
};

static void
xpt2046_corr(int16_t *xp, int16_t *yp)
{
    int x;
    int y;
#if XPT2046_XY_SWAP
    x = *yp;
    y = *xp;
#else
    x = *xp;
    y = *yp;
#endif

    if (x < MYNEWT_VAL(XPT2046_MIN_X)) {
        x = MYNEWT_VAL(XPT2046_MIN_X);
    }
    if (y < MYNEWT_VAL(XPT2046_MIN_Y)) {
        y = MYNEWT_VAL(XPT2046_MIN_Y);
    }

    x = (x - XPT2046_MIN_X) * LV_HOR_RES / XPT2046_X_RANGE;
    y = (y - XPT2046_MIN_Y) * LV_VER_RES / XPT2046_Y_RANGE;

#if XPT2046_X_INV
    x = LV_HOR_RES - x;
#endif

#if XPT2046_Y_INV
    y = LV_VER_RES - y;
#endif
    *xp = x;
    *yp = y;
}

/**
 * Get the current position and state of the touchpad
 * @param data store the read data here
 * @return false: because no ore data to be read
 */
void
xpt2046_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    uint8_t cmd[5] = {0};
    uint8_t buf[5] = {0};
    int16_t x;
    int16_t y;
    (void)indev_drv;

    if (hal_gpio_read(MYNEWT_VAL(XPT2046_INT_PIN)) == 0) {
        cmd[0] = CMD_X_READ;
        cmd[1] = 0;
        cmd[2] = CMD_Y_READ;
        cmd[3] = 0;
        cmd[4] = 0;
        bus_node_duplex_write_read(touch_dev, cmd, buf, 5, 1000, 0);

        /* Normalize */
        x = (((uint16_t)buf[1] << 8) | buf[2]) >> 3;
        y = (((uint16_t)buf[3] << 8) | buf[4]) >> 3;

        /* Convert to screen coordinates */
        xpt2046_corr(&x, &y);

        touch_screen_data.last_x = x;
        touch_screen_data.last_y = y;

        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        x = touch_screen_data.last_x;
        y = touch_screen_data.last_y;
        data->state = LV_INDEV_STATE_RELEASED;
    }

    data->point.x = x;
    data->point.y = y;
}

/**
 * Initialize the XPT2046
 */
void
xpt2046_register_lv_indev(void)
{
    hal_gpio_init_in(MYNEWT_VAL(XPT2046_INT_PIN), HAL_GPIO_PULL_NONE);
    /* Register a keypad input device */
    lv_indev_drv_init(&xpt2046_drv);
    xpt2046_drv.type = LV_INDEV_TYPE_POINTER;
    xpt2046_drv.read_cb = xpt2046_read;
    xpt2046_dev = lv_indev_drv_register(&xpt2046_drv);
}

void
lv_touch_handler(void)
{

}

void
xpt2046_os_dev_create(void)
{
    hal_gpio_init_out(MYNEWT_VAL(XPT2046_SPI_CS_PIN), 1);

    struct bus_node_callbacks cbs = { 0 };
    int rc;

    bus_node_set_callbacks((struct os_dev *)&touch, &cbs);

    rc = bus_spi_node_create("touch", &touch, &touch_spi_cfg, NULL);
    assert(rc == 0);
    touch_dev = os_dev_open("touch", 0, NULL);
}
