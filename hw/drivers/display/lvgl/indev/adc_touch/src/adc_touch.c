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

#include <stdio.h>
#include <os/mynewt.h>
#include <os/os_time.h>
#include <os/os_dev.h>
#include <hal/hal_gpio.h>
#include <bsp/bsp.h>

#include <hal/lv_hal_indev.h>
#include <core/lv_disp.h>
#include <lv_api_map.h>
#include <adc_touch.h>

static adc_dev_t touch_dev;

struct touch_electrodes {
    int xp_pin;
    int yp_pin;
    int xm_pin;
    int ym_pin;
};

struct touch_electrodes electrodes = {
    .xp_pin = MYNEWT_VAL(ADC_TOUCH_XP_PIN),
    .yp_pin = MYNEWT_VAL(ADC_TOUCH_YP_PIN),
    .xm_pin = MYNEWT_VAL(ADC_TOUCH_XM_PIN),
    .ym_pin = MYNEWT_VAL(ADC_TOUCH_YM_PIN),
};

static lv_indev_drv_t adc_drv;
static lv_indev_t *adc_dev;

struct touch_screen_data {
    /* ADC value for left edge */
    uint16_t adc_left;
    /* ADC value for right edge */
    uint16_t adc_right;
    /* ADC value for top edge */
    uint16_t adc_top;
    /* ADC value for bottom edge */
    uint16_t adc_bottom;
    /* Current value for X, Y detected */
    int x;
    int y;
    /* Values that were last reported */
    int last_x;
    int last_y;
};

#define INVERTED_X (MYNEWT_VAL(ADC_TOUCH_ADC_LEFT) > MYNEWT_VAL(ADC_TOUCH_ADC_RIGHT))
#define INVERTED_Y (MYNEWT_VAL(ADC_TOUCH_ADC_TOP) > MYNEWT_VAL(ADC_TOUCH_ADC_BOTTOM))
#define HOR_RES MYNEWT_VAL_LVGL_DISPLAY_HORIZONTAL_RESOLUTION
#define VER_RES MYNEWT_VAL_LVGL_DISPLAY_VERTICAL_RESOLUTION

static struct touch_screen_data touch_screen_data = {
    .adc_left = MYNEWT_VAL(ADC_TOUCH_ADC_LEFT),
    .adc_right = MYNEWT_VAL(ADC_TOUCH_ADC_RIGHT),
    .adc_top = MYNEWT_VAL(ADC_TOUCH_ADC_TOP),
    .adc_bottom = MYNEWT_VAL(ADC_TOUCH_ADC_BOTTOM),
};

void
adc_touch_handler(void)
{
    bool touched;
    int x;
    int y;
    int last_val;
    int nx, ny;
    touch_screen_data.x = -1;
    touch_screen_data.y = -1;

    /* Check if there is short between plates using GPIO
     */
    hal_gpio_init_in(electrodes.yp_pin, HAL_GPIO_PULL_UP);
    hal_gpio_init_in(electrodes.ym_pin, HAL_GPIO_PULL_NONE);
    hal_gpio_init_out(electrodes.xp_pin, 0);
    hal_gpio_init_out(electrodes.xm_pin, 0);

    os_cputime_delay_usecs(MYNEWT_VAL(ADC_TOUCH_ADC_DELAY_US));

    touched = hal_gpio_read(electrodes.ym_pin) == 0;

    if (touched) {
        /* Short detected */

        /* X+ = VCC
         * X- = GND,
         * Y+/Y- as input without pull-up
         */
        hal_gpio_init_out(electrodes.xp_pin, 1);
        hal_gpio_init_in(electrodes.yp_pin, HAL_GPIO_PULL_NONE);
        os_cputime_delay_usecs(MYNEWT_VAL(ADC_TOUCH_ADC_DELAY_US));

        last_val = -1;
        for (nx = 0; nx < 10; nx++) {
            /* Take measurement on Y pin that is routed to ADC channel */
            x = adc_touch_adc_read(touch_dev, electrodes.yp_pin);
            if (abs((x - last_val)) > (x / 16)) {
                last_val = x;
            } else {
                x = (x + last_val) / 2;
                ++nx;
                break;
            }
        }

        /* Sanity check for ADC reading */
        if (x < MYNEWT_VAL_ADC_TOUCH_ADC_X_MIN || x > MYNEWT_VAL_ADC_TOUCH_ADC_X_MAX) {
            goto done;
        }

        /* Y+ = VCC
         * Y- = GND,
         * X+/X- as input without pull-up
         */
        hal_gpio_init_out(electrodes.yp_pin, 1);
        hal_gpio_init_out(electrodes.ym_pin, 0);
        hal_gpio_init_in(electrodes.xp_pin, HAL_GPIO_PULL_NONE);
        hal_gpio_init_in(electrodes.xm_pin, HAL_GPIO_PULL_NONE);

        os_cputime_delay_usecs(MYNEWT_VAL(ADC_TOUCH_ADC_DELAY_US));

        last_val = -1;
        for (ny = 0; ny < 10; ++ny) {
            /* Take measurement on X pin that is routed to ADC channel */
            y = adc_touch_adc_read(touch_dev, electrodes.xp_pin);
            if (abs((y - last_val)) > (y / 16)) {
                last_val = y;
            } else {
                ++ny;
                break;
            }
        }

        /* Sanity check for ADC reading */
        if (y < MYNEWT_VAL_ADC_TOUCH_ADC_Y_MIN || x > MYNEWT_VAL_ADC_TOUCH_ADC_Y_MAX) {
            goto done;
        }

        /* If x is outside what is considered left-right range, adjust range */
        if (INVERTED_X) {
            if (x > touch_screen_data.adc_left) {
                touch_screen_data.adc_left = x;
            } else if (x < touch_screen_data.adc_right) {
                touch_screen_data.adc_right = x;
            }
        } else {
            if (x < touch_screen_data.adc_left) {
                touch_screen_data.adc_left = x;
            } else if (x > touch_screen_data.adc_right) {
                touch_screen_data.adc_right = x;
            }
        }
        /* If y is outside what is considered top-bottom range, adjust range */
        if (INVERTED_Y) {
            if (y > touch_screen_data.adc_top) {
                touch_screen_data.adc_top = y;
            } else if (y < touch_screen_data.adc_bottom) {
                touch_screen_data.adc_bottom = y;
            }
        } else {
            if (y < touch_screen_data.adc_top) {
                touch_screen_data.adc_top = y;
            } else if (y > touch_screen_data.adc_bottom) {
                touch_screen_data.adc_bottom = y;
            }
        }

        /* Convert to display coordinates */
        touch_screen_data.x = (x - touch_screen_data.adc_left) * HOR_RES /
                              (touch_screen_data.adc_right - touch_screen_data.adc_left);
        touch_screen_data.y = (y - touch_screen_data.adc_top) * VER_RES /
                              (touch_screen_data.adc_bottom - touch_screen_data.adc_top);
    }
done:
    hal_gpio_init_out(electrodes.xp_pin, 0);
    hal_gpio_init_out(electrodes.xm_pin, 0);
    hal_gpio_init_out(electrodes.yp_pin, 0);
    hal_gpio_init_out(electrodes.ym_pin, 0);
}

static void
adc_touch_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    adc_touch_handler();

    if (touch_screen_data.x < 0 || touch_screen_data.y < 0) {
        data->point.x = touch_screen_data.last_x;
        data->point.y = touch_screen_data.last_y;
        data->state = LV_INDEV_STATE_REL;
    } else {
        touch_screen_data.last_x = touch_screen_data.x;
        touch_screen_data.last_y = touch_screen_data.y;
        data->point.x = touch_screen_data.last_x;
        data->point.y = touch_screen_data.last_y;
        data->state = LV_INDEV_STATE_PR;
    }
}

void
adc_touch_init(void)
{
    touch_dev = adc_touch_adc_open(electrodes.xp_pin, electrodes.yp_pin);

    /* Register a keypad input device */
    lv_indev_drv_init(&adc_drv);
    adc_drv.type = LV_INDEV_TYPE_POINTER;
    adc_drv.read_cb = adc_touch_read;
    adc_dev = lv_indev_drv_register(&adc_drv);
}
