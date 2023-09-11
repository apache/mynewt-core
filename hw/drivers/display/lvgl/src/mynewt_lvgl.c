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

#include <stdint.h>
#include <syscfg/syscfg.h>
#include <os/os_time.h>
#include <os/os_cputime.h>
#include <os/os_callout.h>
#include <core/lv_refr.h>
#include <lv_glue.h>

static struct os_callout lv_callout;
static os_time_t lv_timer_period;

void lv_timer_cb(struct os_event *ev)
{
    (void)ev;

    lv_refr_now(NULL);
    lv_timer_handler();
    os_callout_reset(&lv_callout, lv_timer_period);
}

static void
init_lv_timer(void)
{
    lv_timer_period = os_time_ms_to_ticks32(MYNEWT_VAL(LVGL_TIMER_PERIOD_MS));
    /*
     * Initialize the callout for a timer event.
     */
    os_callout_init(&lv_callout, os_eventq_dflt_get(),
                    lv_timer_cb, NULL);

    os_callout_reset(&lv_callout, lv_timer_period);
}

#define DISP_DRAW_BUF_LINES MYNEWT_VAL(LV_DISP_DRAW_BUF_LINES)
/* A static or global variable to store the buffers */
static lv_disp_draw_buf_t disp_buf;
/* A variable to hold the drivers. Must be static or global. */
static lv_disp_drv_t disp_drv;
#define DISP_HOR_RES MYNEWT_VAL(LVGL_DISPLAY_HORIZONTAL_RESOLUTION)
#define DISP_VERT_RES MYNEWT_VAL(LVGL_DISPLAY_VERTICAL_RESOLUTION)
/* Static or global buffer(s). The second buffer is optional */
static lv_color_t buf_1[DISP_HOR_RES * DISP_DRAW_BUF_LINES];
#if MYNEWT_VAL(LV_DISP_DOUBLE_BUFFER)
static lv_color_t buf_2[DISP_HOR_RES * DISP_DRAW_BUF_LINES];
#else
#define buf_2 NULL
#endif

/** Extend the invalidated areas to match with the display drivers requirements
 * E.g. round `y` to, 8, 16 ..) on a monochrome display*/
static void
mynewt_lv_rounder(struct _lv_disp_drv_t *driver, lv_area_t *area)
{
    (void)driver;
    uint32_t mask;

    if (MYNEWT_VAL(LV_DISP_X_ALIGN)) {
        mask = (1 << MYNEWT_VAL(LV_DISP_X_ALIGN)) - 1;
        /* Clear left side bits */
        area->x1 &= ~mask;
        /* Set right side bits */
        area->x2 |= mask;
    }
    if (MYNEWT_VAL(LV_DISP_Y_ALIGN)) {
        mask = (1 << MYNEWT_VAL(LV_DISP_Y_ALIGN)) - 1;
        /* Clear top side bits */
        area->y1 &= ~mask;
        /* Set bottom side bits */
        area->y2 |= mask;
    }
}

void
mynewt_lv_init(void)
{
    lv_disp_t *disp;

    init_lv_timer();

    lv_init();

    /*Initialize `disp_buf` with the buffer(s). With only one buffer use NULL instead buf_2 */
    lv_disp_draw_buf_init(&disp_buf, buf_1, buf_2, DISP_HOR_RES * DISP_DRAW_BUF_LINES);
    lv_disp_drv_init(&disp_drv);            /* Basic initialization */
    disp_drv.draw_buf = &disp_buf;          /* Set an initialized buffer */
    disp_drv.hor_res = DISP_HOR_RES;        /* Set the horizontal resolution in pixels */
    disp_drv.ver_res = DISP_VERT_RES;       /* Set the vertical resolution in pixels */
    if (MYNEWT_VAL(LV_DISP_X_ALIGN) || MYNEWT_VAL(LV_DISP_Y_ALIGN)) {
        disp_drv.rounder_cb = mynewt_lv_rounder;
    }

    mynewt_lv_drv_init(&disp_drv);

    /* Register the driver and save the created display objects */
    disp = lv_disp_drv_register(&disp_drv);
    lv_disp_set_default(disp);
}
