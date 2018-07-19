/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * resarding copyright ownership.  The ASF licenses this file
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

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "os/mynewt.h"
#include "hal/hal_spi.h"
#include "hal/hal_i2c.h"
#include "sensor/sensor.h"
#include "sensor/accel.h"
#include "lis2dw12/lis2dw12.h"
#include "lis2dw12_priv.h"
#include "hal/hal_gpio.h"
#include "modlog/modlog.h"
#include "stats/stats.h"
#include <syscfg/syscfg.h>

/*
 * Max time to wait for interrupt.
 */
#define LIS2DW12_MAX_INT_WAIT (4 * OS_TICKS_PER_SEC)

const struct lis2dw12_notif_cfg dflt_notif_cfg[] = {
    {
      .event     = SENSOR_EVENT_TYPE_SINGLE_TAP,
      .int_num   = 0,
      .notif_src = LIS2DW12_INT_SRC_STAP,
      .int_cfg   = LIS2DW12_INT1_CFG_SINGLE_TAP
    },
    {
      .event     = SENSOR_EVENT_TYPE_DOUBLE_TAP,
      .int_num   = 0,
      .notif_src = LIS2DW12_INT_SRC_DTAP,
      .int_cfg   = LIS2DW12_INT1_CFG_DOUBLE_TAP
    },
    {
      .event     = SENSOR_EVENT_TYPE_SLEEP,
      .int_num   = 1,
      .notif_src = LIS2DW12_STATUS_SLEEP_STATE,
      .int_cfg   = LIS2DW12_INT2_CFG_SLEEP_STATE
    },
    {
      .event     = SENSOR_EVENT_TYPE_FREE_FALL,
      .int_num   = 0,
      .notif_src = LIS2DW12_INT_SRC_FF_IA,
      .int_cfg   = LIS2DW12_INT1_CFG_FF
    },
    {
      .event     = SENSOR_EVENT_TYPE_WAKEUP,
      .int_num    = 0,
      .notif_src  = LIS2DW12_INT_SRC_WU_IA,
      .int_cfg    = LIS2DW12_INT1_CFG_WU
    },
    {
      .event     = SENSOR_EVENT_TYPE_SLEEP_CHANGE,
      .int_num   = 1,
      .notif_src = LIS2DW12_INT_SRC_SLP_CHG,
      .int_cfg   = LIS2DW12_INT2_CFG_SLEEP_CHG
    },
    {
      .event     = SENSOR_EVENT_TYPE_ORIENT_CHANGE,
      .int_num   = 0,
      .notif_src = LIS2DW12_SIXD_SRC_6D_IA,
      .int_cfg   = LIS2DW12_INT1_CFG_6D
    },
    {
      .event     = SENSOR_EVENT_TYPE_ORIENT_X_CHANGE,
      .int_num   = 0,
      .notif_src = LIS2DW12_SIXD_SRC_XL|LIS2DW12_SIXD_SRC_XH,
      .int_cfg   = LIS2DW12_INT1_CFG_6D
    },
    {
      .event     = SENSOR_EVENT_TYPE_ORIENT_Y_CHANGE,
      .int_num   = 0,
      .notif_src = LIS2DW12_SIXD_SRC_YL|LIS2DW12_SIXD_SRC_YH,
      .int_cfg   = LIS2DW12_INT1_CFG_6D
    },
    {
      .event     = SENSOR_EVENT_TYPE_ORIENT_Z_CHANGE,
      .int_num   = 0,
      .notif_src = LIS2DW12_SIXD_SRC_ZL|LIS2DW12_SIXD_SRC_ZH,
      .int_cfg   = LIS2DW12_INT1_CFG_6D
    }
};

static struct hal_spi_settings spi_lis2dw12_settings = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE3,
    .baudrate   = 4000,
    .word_size  = HAL_SPI_WORD_SIZE_8BIT,
};

/* Define the stats section and records */
STATS_SECT_START(lis2dw12_stat_section)
#if MYNEWT_VAL(LIS2DW12_NOTIF_STATS)
    STATS_SECT_ENTRY(single_tap_notify)
    STATS_SECT_ENTRY(double_tap_notify)
    STATS_SECT_ENTRY(free_fall_notify)
    STATS_SECT_ENTRY(sleep_notify)
    STATS_SECT_ENTRY(wakeup_notify)
    STATS_SECT_ENTRY(sleep_chg_notify)
    STATS_SECT_ENTRY(orient_chg_notify)
    STATS_SECT_ENTRY(orient_chg_x_notify)
    STATS_SECT_ENTRY(orient_chg_y_notify)
    STATS_SECT_ENTRY(orient_chg_z_notify)
#endif
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(lis2dw12_stat_section)
#if MYNEWT_VAL(LIS2DW12_NOTIF_STATS)
    STATS_NAME(lis2dw12_stat_section, single_tap_notify)
    STATS_NAME(lis2dw12_stat_section, double_tap_notify)
    STATS_NAME(lis2dw12_stat_section, free_fall_notify)
    STATS_NAME(lis2dw12_stat_section, sleep_notify)
    STATS_NAME(lis2dw12_stat_section, wakeup_notify)
    STATS_NAME(lis2dw12_stat_section, sleep_chg_notify)
    STATS_NAME(lis2dw12_stat_section, orient_chg_notify)
    STATS_NAME(lis2dw12_stat_section, orient_chg_x_notify)
    STATS_NAME(lis2dw12_stat_section, orient_chg_y_notify)
    STATS_NAME(lis2dw12_stat_section, orient_chg_z_notify)
#endif
STATS_NAME_END(lis2dw12_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(lis2dw12_stat_section) g_lis2dw12stats;

#define LIS2DW12_LOG(lvl_, ...) \
    MODLOG_ ## lvl_(MYNEWT_VAL(LIS2DW12_LOG_MODULE), __VA_ARGS__)

/* Exports for the sensor API */
static int lis2dw12_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int lis2dw12_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);
static int lis2dw12_sensor_set_notification(struct sensor *,
                                            sensor_event_type_t);
static int lis2dw12_sensor_unset_notification(struct sensor *,
                                              sensor_event_type_t);
static int lis2dw12_sensor_handle_interrupt(struct sensor *);
static int lis2dw12_sensor_set_config(struct sensor *, void *);

static const struct sensor_driver g_lis2dw12_sensor_driver = {
    .sd_read               = lis2dw12_sensor_read,
    .sd_set_config         = lis2dw12_sensor_set_config,
    .sd_get_config         = lis2dw12_sensor_get_config,
    .sd_set_notification   = lis2dw12_sensor_set_notification,
    .sd_unset_notification = lis2dw12_sensor_unset_notification,
    .sd_handle_interrupt   = lis2dw12_sensor_handle_interrupt

};



/**
 * Calculates the acceleration in m/s^2 from mg
 *
 * @param acc value in mg
 * @param float ptr to return calculated value in ms2
 */
void
lis2dw12_calc_acc_ms2(int16_t acc_mg, float *acc_ms2)
{
    *acc_ms2 = (acc_mg * STANDARD_ACCEL_GRAVITY)/1000;
}

/**
 * Calculates the acceleration in mg from m/s^2
 *
 * @param acc value in m/s^2
 * @param int16 ptr to return calculated value in mg
 */
void
lis2dw12_calc_acc_mg(float acc_ms2, int16_t *acc_mg)
{
    *acc_mg = (acc_ms2 * 1000)/STANDARD_ACCEL_GRAVITY;
}


static int lis2dw12_do_read(struct sensor *sensor, sensor_data_func_t data_func,
                            void * data_arg, uint8_t fs)
{
    struct sensor_accel_data sad;
    struct driver_itf *itf;
    int16_t x, y ,z;
    float fx, fy ,fz;
    int rc;

    itf = SENSOR_GET_ITF(sensor);

    x = y = z = 0;

    rc = lis2dw12_get_data(itf, fs, &x, &y, &z);
    if (rc) {
        goto err;
    }

    /* converting values from mg to ms^2 */
    lis2dw12_calc_acc_ms2(x, &fx);
    lis2dw12_calc_acc_ms2(y, &fy);
    lis2dw12_calc_acc_ms2(z, &fz);

    sad.sad_x = fx;
    sad.sad_y = fy;
    sad.sad_z = fz;

    sad.sad_x_is_valid = 1;
    sad.sad_y_is_valid = 1;
    sad.sad_z_is_valid = 1;

    /* Call data function */
    rc = data_func(sensor, data_arg, &sad, SENSOR_TYPE_ACCELEROMETER);
    if (rc != 0) {
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Do accelerometer polling reads
 *
 * @param The sensor ptr
 * @param The sensor type
 * @param The function pointer to invoke for each accelerometer reading.
 * @param The opaque pointer that will be passed in to the function.
 * @param If non-zero, how long the stream should run in milliseconds.
 *
 * @return 0 on success, non-zero on failure.
 */
int
lis2dw12_poll_read(struct sensor *sensor, sensor_type_t sensor_type,
                   sensor_data_func_t data_func, void *data_arg,
                   uint32_t timeout)
{
    struct lis2dw12 *lis2dw12;
    struct lis2dw12_cfg *cfg;
    struct driver_itf *itf;
    uint8_t fs;
    int rc;

    lis2dw12 = (struct lis2dw12 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    cfg = &lis2dw12->cfg;

    /* If the read isn't looking for accel data, don't do anything. */
    if (!(sensor_type & SENSOR_TYPE_ACCELEROMETER)) {
        rc = SYS_EINVAL;
        goto err;
    }

    if (cfg->read_mode.mode != LIS2DW12_READ_M_POLL) {
        rc = SYS_EINVAL;
        goto err;
    }

    rc = lis2dw12_get_fs(itf, &fs);
    if (rc) {
        goto err;
    }

    rc = lis2dw12_do_read(sensor, data_func, data_arg, fs);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

int
lis2dw12_stream_read(struct sensor *sensor,
                     sensor_type_t sensor_type,
                     sensor_data_func_t read_func,
                     void *read_arg,
                     uint32_t time_ms)
{
    struct lis2dw12_pdd *pdd;
    struct lis2dw12 *lis2dw12;
    struct driver_itf *itf;
    struct lis2dw12_cfg *cfg;
    os_time_t time_ticks;
    os_time_t stop_ticks = 0;
    uint8_t fifo_samples;
    uint8_t fs;
    int rc, rc2;

    /* If the read isn't looking for accel data, don't do anything. */
    if (!(sensor_type & SENSOR_TYPE_ACCELEROMETER)) {
        return SYS_EINVAL;
    }

    lis2dw12 = (struct lis2dw12 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &lis2dw12->pdd;
    cfg = &lis2dw12->cfg;

    if (cfg->read_mode.mode != LIS2DW12_READ_M_STREAM) {
        return SYS_EINVAL;
    }

    undo_interrupt(&lis2dw12->intr);

    if (pdd->interrupt) {
        return SYS_EBUSY;
    }

    /* enable interrupt */
    pdd->interrupt = &lis2dw12->intr;

    rc = enable_interrupt(sensor, cfg->read_mode.int_cfg,
                          cfg->read_mode.int_num);
    if (rc) {
        return rc;
    }

    if (time_ms != 0) {
        rc = os_time_ms_to_ticks(time_ms, &time_ticks);
        if (rc) {
            goto err;
        }
        stop_ticks = os_time_get() + time_ticks;
    }

    rc = lis2dw12_get_fs(itf, &fs);
    if (rc) {
        goto err;
    }

    for (;;) {
        /* force at least one read for cases when fifo is disabled */
        rc = wait_interrupt(&lis2dw12->intr, cfg->read_mode.int_num);
        if (rc) {
            goto err;
        }
        fifo_samples = 1;

        while(fifo_samples > 0) {

            /* read all data we beleive is currently in fifo */
            while(fifo_samples > 0) {
                rc = lis2dw12_do_read(sensor, read_func, read_arg, fs);
                if (rc) {
                    goto err;
                }
                fifo_samples--;

            }

            /* check if any data is available in fifo */
            rc = lis2dw12_get_fifo_samples(itf, &fifo_samples);
            if (rc) {
                goto err;
            }

        }

        if (time_ms != 0 && OS_TIME_TICK_GT(os_time_get(), stop_ticks)) {
            break;
        }

    }

err:
    /* disable interrupt */
    pdd->interrupt = NULL;
    rc2 = disable_interrupt(sensor, cfg->read_mode.int_cfg,
                           cfg->read_mode.int_num);
    if (rc) {
        return rc;
    } else {
        return rc2;
    }
}

static int
lis2dw12_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    int rc;
    const struct lis2dw12_cfg *cfg;
    struct lis2dw12 *lis2dw12;
    struct driver_itf *itf;

    /* If the read isn't looking for accel data, don't do anything. */
    if (!(type & SENSOR_TYPE_ACCELEROMETER)) {
        rc = SYS_EINVAL;
        goto err;
    }

    itf = SENSOR_GET_ITF(sensor);

    if (itf->si_type == DRIVER_ITF_SPI) {

        rc = hal_spi_disable(sensor->s_itf.si_num);
        if (rc) {
            goto err;
        }

        rc = hal_spi_config(sensor->s_itf.si_num, &spi_lis2dw12_settings);
        if (rc == EINVAL) {
            /* If spi is already enabled, for nrf52, it returns -1, We should not
             * fail if the spi is already enabled
             */
            goto err;
        }

        rc = hal_spi_enable(sensor->s_itf.si_num);
        if (rc) {
            goto err;
        }
    }

    lis2dw12 = (struct lis2dw12 *)SENSOR_GET_DEVICE(sensor);
    cfg = &lis2dw12->cfg;

    if (cfg->read_mode.mode == LIS2DW12_READ_M_POLL) {
        rc = lis2dw12_poll_read(sensor, type, data_func, data_arg, timeout);
    } else {
        rc = lis2dw12_stream_read(sensor, type, data_func, data_arg, timeout);
    }
err:
    if (rc) {
        return SYS_EINVAL; /* XXX */
    } else {
        return SYS_EOK;
    }
}

static struct lis2dw12_notif_cfg *
lis2dw12_find_notif_cfg_by_event(sensor_event_type_t event,
                                 struct lis2dw12_cfg *cfg)
{
    int i;
    struct lis2dw12_notif_cfg *notif_cfg = NULL;

    if (!cfg) {
        goto err;
    }

    for (i = 0; i < cfg->max_num_notif; i++) {
        if (event == cfg->notif_cfg[i].event) {
            notif_cfg = &cfg->notif_cfg[i];
            break;
        }
    }

    if (i == cfg->max_num_notif) {
       /* here if type is set to a non valid event or more than one event
        * we do not currently support registering for more than one event
        * per notification
        */
        goto err;
    }

    return notif_cfg;
err:
    return NULL;
}

static int
lis2dw12_sensor_set_notification(struct sensor *sensor, sensor_event_type_t event)
{
    struct lis2dw12 *lis2dw12;
    struct lis2dw12_pdd *pdd;
    struct driver_itf *itf;
    struct lis2dw12_notif_cfg *notif_cfg;
    int rc;

    lis2dw12 = (struct lis2dw12 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);
    pdd = &lis2dw12->pdd;

    notif_cfg = lis2dw12_find_notif_cfg_by_event(event, &lis2dw12->cfg);
    if (!notif_cfg) {
        rc = SYS_EINVAL;
        goto err;
    }

    rc = enable_interrupt(sensor, notif_cfg->int_cfg, notif_cfg->int_num);
    if (rc) {
        goto err;
    }

    /* enable double tap detection in wake_up_ths */
    if(event == SENSOR_EVENT_TYPE_DOUBLE_TAP) {
        rc = lis2dw12_set_double_tap_event_en(itf, 1);
        if (rc) {
            goto err;
        }
    }

    pdd->notify_ctx.snec_evtype |= event;

    return 0;
err:
    return rc;
}

static int
lis2dw12_sensor_unset_notification(struct sensor *sensor, sensor_event_type_t event)
{
    struct lis2dw12_notif_cfg *notif_cfg;
    struct lis2dw12 *lis2dw12;
    struct driver_itf *itf;
    int rc;

    lis2dw12 = (struct lis2dw12 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);

    lis2dw12->pdd.notify_ctx.snec_evtype &= ~event;

    if(event == SENSOR_EVENT_TYPE_DOUBLE_TAP) {
        rc = lis2dw12_set_double_tap_event_en(itf, 0);
        if (rc) {
            goto err;
        }
    }

    notif_cfg = lis2dw12_find_notif_cfg_by_event(event, &lis2dw12->cfg);
    if (!notif_cfg) {
        rc = SYS_EINVAL;
        goto err;
    }

    rc = disable_interrupt(sensor, notif_cfg->int_cfg, notif_cfg->int_num);

err:
    return rc;
}

static int
lis2dw12_sensor_set_config(struct sensor *sensor, void *cfg)
{
    struct lis2dw12 *lis2dw12;

    lis2dw12 = (struct lis2dw12 *)SENSOR_GET_DEVICE(sensor);

    return lis2dw12_config(lis2dw12, (struct lis2dw12_cfg*)cfg);
}

static void
lis2dw12_inc_notif_stats(sensor_event_type_t event)
{

#if MYNEWT_VAL(LIS2DW12_NOTIF_STATS)
    switch (event) {
        case SENSOR_EVENT_TYPE_SLEEP:
            STATS_INC(g_lis2dw12stats, sleep_notify);
            break;
        case SENSOR_EVENT_TYPE_SINGLE_TAP:
            STATS_INC(g_lis2dw12stats, single_tap_notify);
            break;
        case SENSOR_EVENT_TYPE_DOUBLE_TAP:
            STATS_INC(g_lis2dw12stats, double_tap_notify);
            break;
        case SENSOR_EVENT_TYPE_ORIENT_CHANGE:
            STATS_INC(g_lis2dw12stats, orient_chg_notify);
            break;
        case SENSOR_EVENT_TYPE_ORIENT_X_CHANGE:
            STATS_INC(g_lis2dw12stats, orient_chg_x_notify);
            break;
        case SENSOR_EVENT_TYPE_ORIENT_Y_CHANGE:
            STATS_INC(g_lis2dw12stats, orient_chg_y_notify);
            break;
        case SENSOR_EVENT_TYPE_ORIENT_Z_CHANGE:
            STATS_INC(g_lis2dw12stats, orient_chg_z_notify);
            break;
        case SENSOR_EVENT_TYPE_SLEEP_CHANGE:
            STATS_INC(g_lis2dw12stats, sleep_chg_notify);
            break;
        case SENSOR_EVENT_TYPE_WAKEUP:
            STATS_INC(g_lis2dw12stats, wakeup_notify);
            break;
        case SENSOR_EVENT_TYPE_FREE_FALL:
            STATS_INC(g_lis2dw12stats, free_fall_notify);
            break;
    }
#endif

    return;
}

static int
lis2dw12_sensor_handle_interrupt(struct sensor *sensor)
{
    struct lis2dw12 *lis2dw12;
    struct driver_itf *itf;
    uint8_t int_src;
    uint8_t int_status;
    uint8_t sixd_src;
    struct lis2dw12_notif_cfg *notif_cfg;
    int rc;

    lis2dw12 = (struct lis2dw12 *)SENSOR_GET_DEVICE(sensor);
    itf = SENSOR_GET_ITF(sensor);

    if (lis2dw12->pdd.notify_ctx.snec_evtype & SENSOR_EVENT_TYPE_SLEEP) {
        /*
         * We need to read this register only if we are
         * interested in the sleep event
         */
        rc = lis2dw12_get_int_status(itf, &int_status);
        if (rc) {
            LIS2DW12_LOG(ERROR, "Could not read int status err=0x%02x\n", rc);
            return rc;
        }

        notif_cfg = lis2dw12_find_notif_cfg_by_event(SENSOR_EVENT_TYPE_SLEEP,
                                                     &lis2dw12->cfg);
        if (!notif_cfg) {
            rc = SYS_EINVAL;
            goto err;
        }

        if (int_status & notif_cfg->notif_src) {
            /* Sleep state detected */
            sensor_mgr_put_notify_evt(&lis2dw12->pdd.notify_ctx,
                                      SENSOR_EVENT_TYPE_SLEEP);
            lis2dw12_inc_notif_stats(SENSOR_EVENT_TYPE_SLEEP);
        }
    }

    rc = lis2dw12_get_sixd_src(itf, &sixd_src);
    if (rc) {
        LIS2DW12_LOG(ERROR, "Could not read sixd src err=0x%02x\n", rc);
        goto err;
    }

    notif_cfg = lis2dw12_find_notif_cfg_by_event(SENSOR_EVENT_TYPE_ORIENT_CHANGE,
                                                 &lis2dw12->cfg);
    if (!notif_cfg) {
        rc = SYS_EINVAL;
        goto err;
    }

    if (sixd_src & notif_cfg->notif_src) {

        /* Orientation change detected, can be a combination of ZH, ZL, YH,
         * YL, XH and XL
         */

        sensor_mgr_put_notify_evt(&lis2dw12->pdd.notify_ctx,
                                  SENSOR_EVENT_TYPE_ORIENT_CHANGE);

        lis2dw12_inc_notif_stats(SENSOR_EVENT_TYPE_ORIENT_CHANGE);
    }

    notif_cfg = lis2dw12_find_notif_cfg_by_event(SENSOR_EVENT_TYPE_ORIENT_X_CHANGE,
                                                 &lis2dw12->cfg);
    if (!notif_cfg) {
        rc = SYS_EINVAL;
        goto err;
    }

    if (sixd_src & notif_cfg->notif_src) {

        /* Orientation change detected, can be a combination of ZH, ZL, YH,
         * YL, XH and XL
         */

        sensor_mgr_put_notify_evt(&lis2dw12->pdd.notify_ctx,
                                  SENSOR_EVENT_TYPE_ORIENT_X_CHANGE);

        lis2dw12_inc_notif_stats(SENSOR_EVENT_TYPE_ORIENT_X_CHANGE);
    }

    notif_cfg = lis2dw12_find_notif_cfg_by_event(SENSOR_EVENT_TYPE_ORIENT_Y_CHANGE,
                                                 &lis2dw12->cfg);
    if (!notif_cfg) {
        rc = SYS_EINVAL;
        goto err;
    }

    if (sixd_src & notif_cfg->notif_src) {

        /* Orientation change detected, can be a combination of ZH, ZL, YH,
         * YL, XH and XL
         */

        sensor_mgr_put_notify_evt(&lis2dw12->pdd.notify_ctx,
                                  SENSOR_EVENT_TYPE_ORIENT_Y_CHANGE);

        lis2dw12_inc_notif_stats(SENSOR_EVENT_TYPE_ORIENT_Y_CHANGE);
    }

    notif_cfg = lis2dw12_find_notif_cfg_by_event(SENSOR_EVENT_TYPE_ORIENT_Z_CHANGE,
                                                 &lis2dw12->cfg);
    if (!notif_cfg) {
        rc = SYS_EINVAL;
        goto err;
    }

    if (sixd_src & notif_cfg->notif_src) {

        /* Orientation change detected, can be a combination of ZH, ZL, YH,
         * YL, XH and XL
         */

        sensor_mgr_put_notify_evt(&lis2dw12->pdd.notify_ctx,
                                  SENSOR_EVENT_TYPE_ORIENT_Z_CHANGE);

        lis2dw12_inc_notif_stats(SENSOR_EVENT_TYPE_ORIENT_Z_CHANGE);
    }

    rc = lis2dw12_clear_int(itf, &int_src);
    if (rc) {
        LIS2DW12_LOG(ERROR, "Could not read int src err=0x%02x\n", rc);
        return rc;
    }

    notif_cfg = lis2dw12_find_notif_cfg_by_event(SENSOR_EVENT_TYPE_SINGLE_TAP,
                                                 &lis2dw12->cfg);
    if (!notif_cfg) {
        rc = SYS_EINVAL;
        goto err;
    }

    if (int_src & notif_cfg->notif_src) {
        /* Single tap is detected */
        sensor_mgr_put_notify_evt(&lis2dw12->pdd.notify_ctx,
                                  SENSOR_EVENT_TYPE_SINGLE_TAP);
        lis2dw12_inc_notif_stats(SENSOR_EVENT_TYPE_SINGLE_TAP);
    }

    notif_cfg = lis2dw12_find_notif_cfg_by_event(SENSOR_EVENT_TYPE_DOUBLE_TAP,
                                                 &lis2dw12->cfg);
    if (!notif_cfg) {
        rc = SYS_EINVAL;
        goto err;
    }

    if (int_src & notif_cfg->notif_src) {
        /* Double tap is detected */
        sensor_mgr_put_notify_evt(&lis2dw12->pdd.notify_ctx,
                                  SENSOR_EVENT_TYPE_DOUBLE_TAP);
        lis2dw12_inc_notif_stats(SENSOR_EVENT_TYPE_DOUBLE_TAP);
    }

    notif_cfg = lis2dw12_find_notif_cfg_by_event(SENSOR_EVENT_TYPE_FREE_FALL,
                                                 &lis2dw12->cfg);
    if (!notif_cfg) {
        rc = SYS_EINVAL;
        goto err;
    }

    if (int_src & notif_cfg->notif_src) {
        /* Freefall is detected */
        sensor_mgr_put_notify_evt(&lis2dw12->pdd.notify_ctx,
                                  SENSOR_EVENT_TYPE_FREE_FALL);
        lis2dw12_inc_notif_stats(SENSOR_EVENT_TYPE_FREE_FALL);
    }

    notif_cfg = lis2dw12_find_notif_cfg_by_event(SENSOR_EVENT_TYPE_WAKEUP,
                                                 &lis2dw12->cfg);
    if (!notif_cfg) {
        rc = SYS_EINVAL;
        goto err;
    }

    if (int_src & notif_cfg->notif_src) {
        /* Wake up is detected */
        sensor_mgr_put_notify_evt(&lis2dw12->pdd.notify_ctx,
                                  SENSOR_EVENT_TYPE_WAKEUP);
        lis2dw12_inc_notif_stats(SENSOR_EVENT_TYPE_WAKEUP);
    }

    notif_cfg = lis2dw12_find_notif_cfg_by_event(SENSOR_EVENT_TYPE_SLEEP_CHANGE,
                                                 &lis2dw12->cfg);
    if (!notif_cfg) {
        rc = SYS_EINVAL;
        goto err;
    }

    if (int_src & notif_cfg->notif_src) {
        /* Sleep change detected, either wakeup or sleep */
        sensor_mgr_put_notify_evt(&lis2dw12->pdd.notify_ctx,
                                  SENSOR_EVENT_TYPE_SLEEP_CHANGE);
        lis2dw12_inc_notif_stats(SENSOR_EVENT_TYPE_SLEEP_CHANGE);
    }

    return 0;
err:
    return rc;
}

static int
lis2dw12_sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{
    int rc;

    if (type != SENSOR_TYPE_ACCELEROMETER) {
        rc = SYS_EINVAL;
        goto err;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT_TRIPLET;

    return 0;
err:
    return rc;
}

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with this accelerometer
 * @param Argument passed to OS device init, unused
 *
 * @return 0 on success, non-zero error on failure.
 */
int
lis2dw12_init(struct os_dev *dev, void *arg)
{
    struct lis2dw12 *lis2dw12;
    struct sensor *sensor;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    lis2dw12 = (struct lis2dw12 *) dev;

    lis2dw12->cfg.mask = SENSOR_TYPE_ALL;

    sensor = &lis2dw12->sensor;

    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_lis2dw12stats),
        STATS_SIZE_INIT_PARMS(g_lis2dw12stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(lis2dw12_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_lis2dw12stats));
    SYSINIT_PANIC_ASSERT(rc == 0);

    rc = sensor_init(sensor, dev);
    if (rc) {
        goto err;
    }

    /* Add the light driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_ACCELEROMETER,
            (struct sensor_driver *) &g_lis2dw12_sensor_driver);
    if (rc) {
        goto err;
    }

    /* Set the interface */
    rc = sensor_set_interface(sensor, arg);
    if (rc) {
        goto err;
    }

    rc = sensor_mgr_register(sensor);
    if (rc) {
        goto err;
    }

    lis2dw12->cfg.notify_ctx.snec_sensor = sensor;

    return 0;
err:
    return rc;

}

/**
 * Configure the sensor
 *
 * @param ptr to sensor driver
 * @param ptr to sensor driver config
 */
int
lis2dw12_config(struct lis2dw12 *lis2dw12, struct lis2dw12_cfg *cfg)
{
    int rc;
    struct driver_itf *itf;
    uint8_t chip_id;
    struct sensor *sensor;

    itf = SENSOR_GET_ITF(&(lis2dw12->sensor));
    sensor = &(lis2dw12->sensor);

    rc = sensor_set_type_mask(&(lis2dw12->sensor), cfg->mask);
    if (rc) {
        goto err;
    }

    lis2dw12->cfg.read_mode.int_cfg = cfg->read_mode.int_cfg;
    lis2dw12->cfg.read_mode.int_num = cfg->read_mode.int_num;
    lis2dw12->cfg.read_mode.mode = cfg->read_mode.mode;

    if (!cfg->notif_cfg) {
        lis2dw12->cfg.notif_cfg = (struct lis2dw12_notif_cfg *)dflt_notif_cfg;
        lis2dw12->cfg.max_num_notif = sizeof(dflt_notif_cfg)/sizeof(*dflt_notif_cfg);
    } else {
        lis2dw12->cfg.notif_cfg = cfg->notif_cfg;
        lis2dw12->cfg.max_num_notif = cfg->max_num_notif;
    }

    lis2dw12->cfg.mask = cfg->mask;

    return 0;
err:
    return rc;
}
