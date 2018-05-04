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

#include "sensor/sensor.h"
#include "sensor/accel.h"

#include "sim/sim_accel.h"

/* Exports for the sensor interface.
 */
static int sim_accel_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int sim_accel_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);

static const struct sensor_driver g_sim_accel_sensor_driver = {
    sim_accel_sensor_read,
    sim_accel_sensor_get_config
};

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with this accelerometer
 * @param Argument passed to OS device init, unused
 *
 * @return 0 on success, non-zero error on failure.
 */
int
sim_accel_init(struct os_dev *dev, void *arg)
{
    struct sim_accel *sa;
    struct sensor *sensor;
    int rc;

    sa = (struct sim_accel *) dev;

    sensor = &sa->sa_sensor;

    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        goto err;
    }

    rc = sensor_set_driver(sensor, SENSOR_TYPE_ACCELEROMETER,
            (struct sensor_driver *) &g_sim_accel_sensor_driver);
    if (rc != 0) {
        goto err;
    }

    rc = sensor_mgr_register(sensor);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

int
sim_accel_config(struct sim_accel *sa, struct sim_accel_cfg *cfg)
{
    /* Overwrite the configuration associated with this generic accelleromter. */
    memcpy(&sa->sa_cfg, cfg, sizeof(*cfg));

    return (0);
}

static int
sim_accel_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    struct sim_accel *sa;
    struct sensor_accel_data sad;
    os_time_t now;
    uint32_t num_samples;
    int i;
    int rc;

    /* If the read isn't looking for accel data, then don't do anything. */
    if (!(type & SENSOR_TYPE_ACCELEROMETER)) {
        rc = SYS_EINVAL;
        goto err;
    }

    sa = (struct sim_accel *) SENSOR_GET_DEVICE(sensor);

    /* When a sensor is "read", we get the last 'n' samples from the device
     * and pass them to the sensor data function.  Based on the sample
     * interval provided to sim_accel_config() and the last time this function
     * was called, 'n' samples are generated.
     */
    now = os_time_get();

    num_samples = (now - sa->sa_last_read_time) / sa->sa_cfg.sac_sample_itvl;
    num_samples = min(num_samples, sa->sa_cfg.sac_nr_samples);

    /* By default only readings are provided for 1-axis (x), however,
     * if number of axises is configured, up to 3-axises of data can be
     * returned.
     */
    sad.sad_x = 0.0;
    sad.sad_y = 0.0;
    sad.sad_z = 0.0;

    sad.sad_x_is_valid = 1;
    sad.sad_y_is_valid = 0;
    sad.sad_z_is_valid = 0;

    if (sa->sa_cfg.sac_nr_axises > 1) {
        sad.sad_y = 0.0;
    }
    if (sa->sa_cfg.sac_nr_axises > 2) {
        sad.sad_z = 0.0;
    }

    /* Call data function for each of the generated readings. */
    for (i = 0; i < num_samples; i++) {
        rc = data_func(sensor, data_arg, &sad, SENSOR_TYPE_ACCELEROMETER);
        if (rc != 0) {
            goto err;
        }
    }

    return (0);
err:
    return (rc);
}

static int
sim_accel_sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{
    int rc;

    if (type != SENSOR_TYPE_ACCELEROMETER) {
        rc = SYS_EINVAL;
        goto err;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT_TRIPLET;

    return (0);
err:
    return (rc);
}

