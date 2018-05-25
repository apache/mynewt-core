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

#include <string.h>
#include <errno.h>
#include <assert.h>

#include "os/mynewt.h"
#include "hal/hal_i2c.h"
#include "hal/hal_gpio.h"
#include "drv2605/drv2605.h"
#include "drv2605_priv.h"

#if MYNEWT_VAL(DRV2605_LOG)
#include "log/log.h"
#endif

#if MYNEWT_VAL(DRV2605_STATS)
#include "stats/stats.h"
#endif

#if MYNEWT_VAL(DRV2605_STATS)
/* Define the stats section and records */
STATS_SECT_START(drv2605_stat_section)
    STATS_SECT_ENTRY(errors)
STATS_SECT_END

/* Define stat names for querying */
STATS_NAME_START(drv2605_stat_section)
    STATS_NAME(drv2605_stat_section, errors)
STATS_NAME_END(drv2605_stat_section)

/* Global variable used to hold stats data */
STATS_SECT_DECL(drv2605_stat_section) g_drv2605stats;
#endif

#if MYNEWT_VAL(DRV2605_LOG)
#define LOG_MODULE_DRV2605 (306)
#define DRV2605_INFO(...)  LOG_INFO(&_log, LOG_MODULE_DRV2605, __VA_ARGS__)
#define DRV2605_ERR(...)   LOG_ERROR(&_log, LOG_MODULE_DRV2605, __VA_ARGS__)
static struct log _log;
#else
#define DRV2605_INFO(...)
#define DRV2605_ERR(...)
#endif


/**
 * Writes a single byte to the specified register
 *
 * @param The Sensor interface
 * @param The register address to write to
 * @param The value to write
 *
 * @return 0 on success, non-zero error on failure.
 */
int
drv2605_write8(struct sensor_itf *itf, uint8_t reg, uint8_t value)
{
    int rc;
    uint8_t payload[2] = { reg, value};

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 2,
        .buffer = payload
    };

    rc = hal_i2c_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC, 1);
    if (rc) {
        DRV2605_ERR("Failed to write to 0x%02X:0x%02X with value 0x%02X\n",
                       data_struct.address, reg, value);
#if MYNEWT_VAL(DRV2605_STATS)
        STATS_INC(g_drv2605stats, errors);
#endif
    }

    return rc;
}

/**
 * Writes multiple bytes starting at the specified register (MAX: 8 bytes)
 *
 * @param The Sensor interface
 * @param The register address to write to
 * @param The data buffer to write from
 *
 * @return 0 on success, non-zero error on failure.
 */
int
drv2605_writelen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer,
                      uint8_t len)
{
    int rc;
    uint8_t payload[9] = { reg, 0, 0, 0, 0, 0, 0, 0, 0};

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = len + 1,
        .buffer = payload
    };

    if (len > (sizeof(payload) - 1)) {
        rc = OS_EINVAL;
        goto err;
    }

    memcpy(&payload[1], buffer, len);

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        DRV2605_ERR("I2C access failed at address 0x%02X\n", data_struct.address);
#if MYNEWT_VAL(DRV2605_STATS)
        STATS_INC(g_drv2605stats, errors);
#endif
        goto err;
    }

    return 0;
err:
    return rc;
}

/**
 * Reads a single byte from the specified register
 *
 * @param The Sensor interface
 * @param The register address to read from
 * @param Pointer to where the register value should be written
 *
 * @return 0 on success, non-zero error on failure.
 */
int
drv2605_read8(struct sensor_itf *itf, uint8_t reg, uint8_t *value)
{
    int rc;
    uint8_t payload;

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = &payload
    };

    /* Register write */
    payload = reg;
    rc = hal_i2c_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 0);
    if (rc) {
        DRV2605_ERR("I2C register write failed at address 0x%02X:0x%02X\n",
                   data_struct.address, reg);
#if MYNEWT_VAL(DRV2605_STATS)
        STATS_INC(g_drv2605stats, errors);
#endif
        goto err;
    }

    /* Read one byte back */
    payload = 0;
    rc = hal_i2c_master_read(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1);
    *value = payload;
    if (rc) {
        DRV2605_ERR("Failed to read from 0x%02X:0x%02X\n", data_struct.address, reg);
#if MYNEWT_VAL(DRV2605_STATS)
        STATS_INC(g_drv2605stats, errors);
#endif
    }

err:
    return rc;
}

/**
 * Read data from the sensor of variable length (MAX: 8 bytes)
 *
 * @param The Sensor interface
 * @param Register to read from
 * @param Buffer to read into
 * @param Length of the buffer
 *
 * @return 0 on success and non-zero on failure
 */
int
drv2605_readlen(struct sensor_itf *itf, uint8_t reg, uint8_t *buffer,
               uint8_t len)
{
    int rc;
    uint8_t payload[23] = { reg, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0};

    struct hal_i2c_master_data data_struct = {
        .address = itf->si_addr,
        .len = 1,
        .buffer = payload
    };

    /* Clear the supplied buffer */
    memset(buffer, 0, len);

    /* Register write */
    rc = hal_i2c_master_write(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 0);
    if (rc) {
        DRV2605_ERR("I2C access failed at address 0x%02X\n", data_struct.address);
#if MYNEWT_VAL(DRV2605_STATS)
        STATS_INC(g_drv2605stats, errors);
#endif
        goto err;
    }

    /* Read len bytes back */
    memset(payload, 0, sizeof(payload));
    data_struct.len = len;
    rc = hal_i2c_master_read(itf->si_num, &data_struct, OS_TICKS_PER_SEC / 10, 1);
    if (rc) {
        DRV2605_ERR("Failed to read from 0x%02X:0x%02X\n", data_struct.address, reg);
#if MYNEWT_VAL(DRV2605_STATS)
        STATS_INC(g_drv2605stats, errors);
#endif
        goto err;
    }

    /* Copy the I2C results into the supplied buffer */
    memcpy(buffer, payload, len);

    return 0;
err:
    return rc;
}

//  general best fit values from datasheet 8.5.6
int
drv2605_default_cal(struct drv2605_cal *cal)
{
    cal->brake_factor = 2;
    cal->loop_gain = 2;
    cal->lra_sample_time = 3;
    cal->lra_blanking_time = 1;
    cal->lra_idiss_time = 1;
    cal->auto_cal_time = 3;
    cal->lra_zc_det_time = 0;

    return 0;
}

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with this haptic feedback controller
 * @param Argument passed to OS device init, unused
 *
 * @return 0 on success, non-zero error on failure.
 */
int
drv2605_init(struct os_dev *dev, void *arg)
{
    struct drv2605 *drv2605;
    struct sensor *sensor;
    uint8_t id;
    int rc;

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    drv2605 = (struct drv2605 *) dev;

#if MYNEWT_VAL(DRV2605_LOG)
    log_register(dev->od_name, &_log, &log_console_handler, NULL, LOG_SYSLEVEL);
#endif

    sensor = &drv2605->sensor;

#if MYNEWT_VAL(DRV2605_STATS)
    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_drv2605stats),
        STATS_SIZE_INIT_PARMS(g_drv2605stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(drv2605_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_drv2605stats));
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        goto err;
    }

    /* Set the interface */
    rc = sensor_set_interface(sensor, arg);
    if (rc) {
        goto err;
    }

    /* Check if we can read the chip address */
    rc = drv2605_get_chip_id(arg, &id);
    if (rc) {
        DRV2605_ERR("unable to get chip id [1]: %d\n", rc);
        goto err;
    }

    if (id != DRV2605_STATUS_DEVICE_ID_2605 && id != DRV2605_STATUS_DEVICE_ID_2605L) {
        os_time_delay((OS_TICKS_PER_SEC * 100)/1000 + 1);

        rc = drv2605_get_chip_id(arg, &id);
        if (rc) {
            DRV2605_ERR("unable to get chip id [2]: %d\n", rc);
            goto err;
        }

        if (id != DRV2605_STATUS_DEVICE_ID_2605 && id != DRV2605_STATUS_DEVICE_ID_2605L) {
            rc = SYS_EINVAL;
            DRV2605_ERR("id not as expected: got: %d, expected %d or %d\n", id,
                        DRV2605_STATUS_DEVICE_ID_2605, DRV2605_STATUS_DEVICE_ID_2605L);
            goto err;
        }
    }

    return (0);
err:
    DRV2605_ERR("Error initializing DRV2605: %d\n", rc);
    return (rc);
}

/**
 * Get chip ID from the sensor
 *
 * @param The sensor interface
 * @param Pointer to the variable to fill up chip ID in
 * @return 0 on success, non-zero on failure
 */
int
drv2605_get_chip_id(struct sensor_itf *itf, uint8_t *id)
{
    int rc;
    uint8_t idtmp;

    /* Check if we can read the chip address */
    rc = drv2605_read8(itf, DRV2605_STATUS_ADDR, &idtmp);
    if (rc) {
        goto err;
    }

    *id = (idtmp & DRV2605_STATUS_DEVICE_ID_MASK) >> DRV2605_STATUS_DEVICE_ID_POS;

    return 0;
err:
    return rc;
}


// NOTE diagnostics (and frankly all operation) will in all likelihood fail
// if your motor is not SECURED to a mass.  It can't be floating on your desk
// even for prototyping
int
drv2605_mode_diagnostic(struct sensor_itf *itf)
{
    int rc;
    uint8_t temp;
    uint8_t interval = 255;
    uint8_t last_mode;

    rc = drv2605_read8(itf, DRV2605_MODE_ADDR, &last_mode);
    if (rc) {
        goto err;
    }

    rc = drv2605_write8(itf, DRV2605_MODE_ADDR, DRV2605_MODE_DIAGNOSTICS | DRV2605_MODE_ACTIVE);
    if (rc) {
        goto err;
    }

    // 4. Set the GO bit (write 0x01 to register 0x0C) to start the auto-calibration process
    rc = drv2605_write8(itf, DRV2605_GO_ADDR, DRV2605_GO_GO);
    if (rc) {
        goto err;
    }

    // When diagnostic is complete, the GO bit automatically clears. Timeout after 255 x 5ms or 1275ms
    do{
        os_time_delay((OS_TICKS_PER_SEC * 5)/1000 + 1);
        rc = drv2605_read8(itf, DRV2605_GO_ADDR, &temp);
    } while (!rc && interval-- && (temp & DRV2605_GO_GO));

    // if we timed out
    if (!interval) {
        rc = OS_TIMEOUT;
        goto err;
    }

    // 5. Check the status of the DIAG_RESULT bit (in register 0x00) to ensure that the diagnostic routine is complete without faults.
    rc = drv2605_read8(itf, DRV2605_STATUS_ADDR, &temp);
    if (rc || (temp & DRV2605_STATUS_DIAG_RESULT_FAIL)) {
        rc = OS_ERROR;
        goto err;
    }

    // put back into standby like all other successful mode ops
    rc = drv2605_write8(itf, DRV2605_MODE_ADDR, (last_mode & (~DRV2605_MODE_STANDBY_MASK)) | DRV2605_MODE_STANDBY);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}


int
drv2605_send_defaults(struct sensor_itf *itf, struct drv2605_cfg *cfg)
{
    int rc;

    rc = drv2605_write8(itf, DRV2605_RATED_VOLTAGE_ADDR, MYNEWT_VAL(DRV2605_RATED_VOLTAGE));
    if (rc) {
        goto err;
    }

    rc = drv2605_write8(itf, DRV2605_OVERDRIVE_CLAMP_VOLTAGE_ADDR, MYNEWT_VAL(DRV2605_OD_CLAMP));
    if (rc) {
        goto err;
    }

    uint8_t motor_mask = 0;
    if (cfg->motor_type == DRV2605_MOTOR_LRA) {
        motor_mask = DRV2605_FEEDBACK_CONTROL_N_LRA;
    }
    else {
        motor_mask = DRV2605_FEEDBACK_CONTROL_N_ERM;
    }
    rc = drv2605_write8(itf, DRV2605_FEEDBACK_CONTROL_ADDR, ((MYNEWT_VAL(DRV2605_CALIBRATED_BEMF_GAIN) & DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_MAX) << DRV2605_FEEDBACK_CONTROL_BEMF_GAIN_POS) | motor_mask );
    if (rc) {
        goto err;
    }

    // They seem to always enable startup boost in the dev kit so throw it in?
    rc = drv2605_write8(itf, DRV2605_CONTROL1_ADDR, ((MYNEWT_VAL(DRV2605_DRIVE_TIME) & DRV2605_CONTROL1_DRIVE_TIME_MAX) << DRV2605_CONTROL1_DRIVE_TIME_POS) | DRV2605_CONTROL1_STARTUP_BOOST_ENABLE);
    if (rc) {
        goto err;
    }

    // TODO: the selection of LRA vs ERM could also include open vs. closed loop, allowing the
    // full matrix of possibilities
    if (cfg->motor_type == DRV2605_MOTOR_LRA) {
        rc = drv2605_write8(itf, DRV2605_CONTROL3_ADDR, DRV2605_CONTROL3_LRA_DRIVE_MODE_ONCE | DRV2605_CONTROL3_LRA_OPEN_LOOP_CLOSED);
    }
    else {
        rc = drv2605_write8(itf, DRV2605_CONTROL3_ADDR, DRV2605_CONTROL3_ERM_OPEN_LOOP_ENABLED);
    }
    if (rc) {
        goto err;
    }

    rc = drv2605_write8(itf, DRV2605_AUTO_CALIBRATION_COMPENSATION_RESULT_ADDR, MYNEWT_VAL(DRV2605_CALIBRATED_COMP));
    if (rc) {
        goto err;
    }

    rc = drv2605_write8(itf, DRV2605_AUTO_CALIBRATION_BACK_EMF_RESULT_ADDR, MYNEWT_VAL(DRV2605_CALIBRATED_BEMF));
    if (rc) {
        goto err;
    }

    // Library selection occurs through register 0x03 (see the (Address: 0x03) section).
    uint8_t library_selection;
    if (cfg->motor_type == DRV2605_MOTOR_LRA) {
        // Library 6 is a closed-loop library tuned for LRAs.
        library_selection = DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_LRA;
    }
    else {
        // TODO: there could be a setter function for the ERM library choices
        // Library B is an open-loop ERM set for 3V
        library_selection = DRV2605_WAVEFORM_CONTROL_LIBRARY_SEL_B;
    }
    rc = drv2605_write8(itf, DRV2605_WAVEFORM_CONTROL_ADDR, library_selection);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

int
drv2605_get_power_mode(struct sensor_itf *itf, enum drv2605_power_mode *power_mode)
{
    int rc;
    uint8_t last_mode;
    bool standby, en_pin;

    rc = drv2605_read8(itf, DRV2605_MODE_ADDR, &last_mode);
    if (rc) {
        goto err;
    }

    standby = last_mode & DRV2605_MODE_STANDBY_MASK;
    en_pin = hal_gpio_read(itf->si_cs_pin);

    if(!en_pin){
        *power_mode = DRV2605_POWER_OFF;
    }else{
        if(standby){
            *power_mode = DRV2605_POWER_STANDBY;
        }else{
            *power_mode = DRV2605_POWER_ACTIVE;
        }
    }

    return 0;
err:
    return rc;
}

int
drv2605_set_standby(struct sensor_itf *itf, bool standby)
{
    int rc;
    uint8_t last_mode;

    rc = drv2605_read8(itf, DRV2605_MODE_ADDR, &last_mode);
    if (rc) {
        goto err;
    }

    uint8_t mode;
    if(standby){
        mode = DRV2605_MODE_STANDBY;
    }else{
        mode = DRV2605_MODE_ACTIVE;
    }

    rc = drv2605_write8(itf, DRV2605_MODE_ADDR, (last_mode & (~DRV2605_MODE_STANDBY_MASK)) | mode);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

int
drv2605_set_power_mode(struct sensor_itf *itf, enum drv2605_power_mode power_mode)
{
    // TODO: any hiccup in writing enable if already active? dont like the idea of reading it first though..
    switch(power_mode) {
        case DRV2605_POWER_STANDBY:
            hal_gpio_write(itf->si_cs_pin, 1);
            return drv2605_set_standby(itf, 1);
        case DRV2605_POWER_ACTIVE:
            hal_gpio_write(itf->si_cs_pin, 1);
            return drv2605_set_standby(itf, 0);
        case DRV2605_POWER_OFF:
            hal_gpio_write(itf->si_cs_pin, 0);
            return 0;
        default:
            return SYS_EINVAL;
    }
}

int
drv2605_validate_cal(struct drv2605_cal *cal)
{
    int rc;

    if (cal->brake_factor > DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_MAX) {
        rc = OS_EINVAL;
        goto err;
    }

    if (cal->loop_gain > DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_MAX) {
        rc = OS_EINVAL;
        goto err;
    }

    if (cal->lra_sample_time > DRV2605_CONTROL2_SAMPLE_TIME_MAX) {
        rc = OS_EINVAL;
        goto err;
    }

    if (cal->lra_blanking_time > DRV2605_BLANKING_TIME_MAX) {
        rc = OS_EINVAL;
        goto err;
    }

    if (cal->lra_idiss_time > DRV2605_IDISS_TIME_MAX) {
        rc = OS_EINVAL;
        goto err;
    }

    if (cal->auto_cal_time > DRV2605_CONTROL4_AUTO_CAL_TIME_MAX) {
        rc = OS_EINVAL;
        goto err;
    }

    if (cal->lra_zc_det_time > DRV2605_CONTROL4_ZC_DET_TIME_MAX) {
        rc = OS_EINVAL;
        goto err;
    }

    return 0;
err:
    return rc;
}

// if succesful calibration overrites DRV2605_BEMF_GAIN, DRV2605_CALIBRATED_COMP and DRV2605_CALIBRATED_BEMF
int
drv2605_mode_calibrate(struct sensor_itf *itf, struct drv2605_cal *cal)
{
    int rc;
    uint8_t temp;
    uint8_t interval = 255;
    uint8_t last_fb, last_mode;

    rc = drv2605_read8(itf, DRV2605_MODE_ADDR, &last_mode);
    if (rc) {
        goto err;
    }

    rc = drv2605_validate_cal(cal);
    if (rc) {
        goto err;
    }

    rc = drv2605_read8(itf, DRV2605_FEEDBACK_CONTROL_ADDR, &last_fb);
    if (rc) {
        goto err;
    }

    // technically only need to protect the ERM_LRA bit as DRV2605_BEMF_GAIN will be altered anyway, but lets show em our fancy bit math
    uint8_t mask = (DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_MASK | DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_MASK);
    uint8_t altered = (cal->brake_factor << DRV2605_FEEDBACK_CONTROL_FB_BRAKE_FACTOR_POS) | (cal->loop_gain << DRV2605_FEEDBACK_CONTROL_LOOP_GAIN_POS);
    uint8_t new = (last_fb & (~mask)) | altered;
    rc = drv2605_write8(itf, DRV2605_FEEDBACK_CONTROL_ADDR, new);
    if (rc) {
        goto err;
    }

    int8_t idiss_lsb = cal->lra_idiss_time & 0x03;
    int8_t blanking_lsb = cal->lra_blanking_time & 0x03;
    int8_t ctrl2 = (cal->lra_sample_time << DRV2605_CONTROL2_SAMPLE_TIME_POS) | (blanking_lsb << DRV2605_CONTROL2_BLANKING_TIME_LSB_POS) | (idiss_lsb << DRV2605_CONTROL2_IDISS_TIME_LSB_POS);
    rc = drv2605_write8(itf, DRV2605_CONTROL2_ADDR, ctrl2);
    if (rc) {
        goto err;
    }

    int8_t blanking_msb = cal->lra_blanking_time & 0x0C;
    int8_t idiss_msb = cal->lra_idiss_time & 0x0C;
    rc = drv2605_write8(itf, DRV2605_CONTROL5_ADDR, blanking_msb  << DRV2605_CONTROL5_BLANKING_TIME_MSB_POS | idiss_msb  << DRV2605_CONTROL5_IDISS_TIME_MSB_POS );
    if (rc) {
        goto err;
    }

    rc = drv2605_write8(itf, DRV2605_CONTROL4_ADDR, (cal->lra_zc_det_time << DRV2605_CONTROL4_ZC_DET_TIME_POS) | (cal->auto_cal_time << DRV2605_CONTROL4_AUTO_CAL_TIME_POS));
    if (rc) {
        goto err;
    }

    // 2. Write a value of 0x07 to register 0x01. This value moves the DRV2605L device out of STANDBY and places the MODE[2:0] bits in auto-calibration mode.
    rc = drv2605_write8(itf, DRV2605_MODE_ADDR, DRV2605_MODE_AUTO_CALIBRATION | DRV2605_MODE_ACTIVE);
    if (rc) {
        goto err;
    }

    // 4. Set the GO bit (write 0x01 to register 0x0C) to start the auto-calibration process.
    rc = drv2605_write8(itf, DRV2605_GO_ADDR, DRV2605_GO_GO);
    if (rc) {
        goto err;
    }

    // When auto calibration is complete, the GO bit automatically clears. The auto-calibration results are written in the respective registers as shown in Figure 25.
    do{
        os_time_delay((OS_TICKS_PER_SEC * 5)/1000 + 1);
        rc = drv2605_read8(itf, DRV2605_GO_ADDR, &temp);
    } while (!rc && interval-- && (temp & DRV2605_GO_GO));

    // if we timed out
    if (!interval) {
        rc = OS_TIMEOUT;
        goto err;
    }

    // 5. Check the status of the DIAG_RESULT bit (in register 0x00) to ensure that the auto-calibration routine is complete without faults
    rc = drv2605_read8(itf, DRV2605_STATUS_ADDR, &temp);
    if (rc || (temp & DRV2605_STATUS_DIAG_RESULT_FAIL)) {
        rc = OS_ERROR;
        goto err;
    }

    // put back into standby like all other successful mode ops
    rc = drv2605_write8(itf, DRV2605_MODE_ADDR, (last_mode & (~DRV2605_MODE_STANDBY_MASK)) | DRV2605_MODE_STANDBY);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

int
drv2605_mode_rom(struct sensor_itf *itf)
{
    int rc;

    rc = drv2605_write8(itf, DRV2605_MODE_ADDR, DRV2605_MODE_INTERNAL_TRIGGER | DRV2605_MODE_STANDBY);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

int
drv2605_mode_rtp(struct sensor_itf *itf)
{
    int rc;

    rc = drv2605_write8(itf, DRV2605_MODE_ADDR, DRV2605_MODE_RTP | DRV2605_MODE_STANDBY);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

int
drv2605_mode_pwm(struct sensor_itf *itf)
{
    int rc;

    rc = drv2605_write8(itf, DRV2605_MODE_ADDR, DRV2605_MODE_PWM_ANALOG_INPUT | DRV2605_MODE_STANDBY);
    if (rc) {
        goto err;
    }

    rc = drv2605_write8(itf, DRV2605_CONTROL3_ADDR, DRV2605_CONTROL3_N_PWM_ANALOG_MASK);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

// NOTE reset sets mode back to standby
// obviously you must _configure again after a reset
int
drv2605_mode_reset(struct sensor_itf *itf)
{
    int rc;
    uint8_t temp;
    uint8_t interval = 255;

    rc = drv2605_write8(itf, DRV2605_MODE_ADDR, DRV2605_MODE_RESET);
    if (rc) {
        goto err;
    }

    // When reset is complete, the reset bit automatically clears. Timeout after 255 x 5ms or 1275ms
    do{
        os_time_delay((OS_TICKS_PER_SEC * 5)/1000 + 1);
        rc = drv2605_read8(itf, DRV2605_MODE_ADDR, &temp);
    } while (!rc && interval-- && (temp & DRV2605_MODE_RESET));

    // if we timed out
    if (!interval) {
        rc = OS_TIMEOUT;
        goto err;
    }

    return 0;
err:
    return rc;
}

// note device MUST be reconfigured for an operational state after a an
// error or after succsessful diag and calibration and reset upon success
// the device is always left in DRV2605_POWER_STANDBY state no device state
// is guaranteed for error returns
int
drv2605_config(struct drv2605 *drv2605, struct drv2605_cfg *cfg)
{
    int rc;
    struct sensor_itf *itf;

    itf = SENSOR_GET_ITF(&(drv2605->sensor));

    rc = hal_gpio_init_out(itf->si_cs_pin, 1);
    if (rc) {
        return rc;
    }

    rc = drv2605_send_defaults(itf, cfg);
    if (rc) {
        return rc;
    }

    switch(cfg->op_mode) {
        case DRV2605_OP_ROM:
            return drv2605_mode_rom(itf);
        case DRV2605_OP_PWM:
            return drv2605_mode_pwm(itf);
        case DRV2605_OP_ANALOG:
            return drv2605_mode_pwm(itf);
        case DRV2605_OP_RTP:
            return drv2605_mode_rtp(itf);
        case DRV2605_OP_DIAGNOSTIC:
            return drv2605_mode_diagnostic(itf);
        case DRV2605_OP_CALIBRATION:
            return drv2605_mode_calibrate(itf, &cfg->cal);
        case DRV2605_OP_RESET:
            return drv2605_mode_reset(itf);
        default:
            return SYS_EINVAL;
    }
}

int
drv2605_load_rom(struct sensor_itf *itf, uint8_t* wav_ids, size_t len)
{
    int rc;

    if (len > 8) {
        rc = SYS_EINVAL;
        goto err;
    }

    rc = drv2605_writelen(itf, DRV2605_WAVEFORM_SEQUENCER_ADDR, wav_ids, len);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

int
drv2605_trigger_rom(struct sensor_itf *itf)
{
    return drv2605_write8(itf, DRV2605_GO_ADDR, DRV2605_GO_GO);
}

// theres sadly no interrupt for knowing when long running roms are finished, youll need to block on this or set a callout
// to poll for completion
int
drv2605_rom_busy(struct sensor_itf *itf, bool *status)
{
    int rc;
    uint8_t temp;

    rc = drv2605_read8(itf, DRV2605_GO_ADDR, &temp);
    if (rc) {
        goto err;
    }

    *status = temp;
    return 0;
err:
    return rc;
}

int
drv2605_load_rtp(struct sensor_itf *itf, uint8_t value)
{
    int rc;

    rc = drv2605_write8(itf, DRV2605_REAL_TIME_PLAYBACK_INPUT_ADDR, value);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}
