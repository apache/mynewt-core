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

#include "defs/error.h"

#include "os/os.h"
#include "sysinit/sysinit.h"

#include "sensor/sensor.h"
#include "sensor/mag.h"

#include "sim/sim_mag.h"

/* Exports for the sensor interface.
 */
static int sim_mag_sensor_read(struct sensor *, sensor_type_t,
        sensor_data_func_t, void *, uint32_t);
static int sim_mag_sensor_get_config(struct sensor *, sensor_type_t,
        struct sensor_cfg *);

static const struct sensor_driver g_sim_mag_sensor_driver = {
    sim_mag_sensor_read,
    sim_mag_sensor_get_config
};

/**
 * Expects to be called back through os_dev_create().
 *
 * @param The device object associated with this magnetometer
 * @param Argument passed to OS device init, unused
 *
 * @return 0 on success, non-zero error on failure.
 */
int
sim_mag_init(struct os_dev *dev, void *arg)
{
    struct sim_mag *sm;
    struct sensor *sensor;
    int rc;

    sm = (struct sim_mag *) dev;

    sensor = &sm->sm_sensor;

    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        goto err;
    }

    rc = sensor_set_driver(sensor, SENSOR_TYPE_MAGNETIC_FIELD,
            (struct sensor_driver *) &g_sim_mag_sensor_driver);
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
sim_mag_config(struct sim_mag *sm, struct sim_mag_cfg *cfg)
{
    /* Overwrite the configuration associated with this generic magnetometer. */
    memcpy(&sm->sm_cfg, cfg, sizeof(*cfg));

    return (0);
}

static int
sim_mag_sensor_read(struct sensor *sensor, sensor_type_t type,
        sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    struct sim_mag *sm;
    struct sensor_mag_data smd;
    os_time_t now;
    uint32_t num_samples;
    int i;
    int rc;

    /* If the read isn't looking for mag data, then don't do anything. */
    if (!(type & SENSOR_TYPE_MAGNETIC_FIELD)) {
        rc = SYS_EINVAL;
        goto err;
    }

    sm = (struct sim_mag *) SENSOR_GET_DEVICE(sensor);

    /* When a sensor is "read", we get the last 'n' samples from the device
     * and pass them to the sensor data function.  Based on the sample
     * interval provided to sim_mag_config() and the last time this function
     * was called, 'n' samples are generated.
     */
    now = os_time_get();

    num_samples = (now - sm->sm_last_read_time) / sm->sm_cfg.smc_sample_itvl;
    num_samples = min(num_samples, sm->sm_cfg.smc_nr_samples);

    /* By default only readings are provided for 1-axis (x), however,
     * if number of axises is configured, up to 3-axises of data can be
     * returned.
     */
    smd.smd_x = 0.0;
    smd.smd_y = 0.0;
    smd.smd_z = 0.0;

    smd.smd_x_is_valid = 1;
    smd.smd_y_is_valid = 0;
    smd.smd_z_is_valid = 0;

    if (sm->sm_cfg.smc_nr_axises > 1) {
        smd.smd_y = 0.0;
    }
    if (sm->sm_cfg.smc_nr_axises > 2) {
        smd.smd_z = 0.0;
    }

    /* Call data function for each of the generated readings. */
    for (i = 0; i < num_samples; i++) {
        rc = data_func(sensor, data_arg, &smd);
        if (rc != 0) {
            goto err;
        }
    }

    return (0);
err:
    return (rc);
}

static int
sim_mag_sensor_get_config(struct sensor *sensor, sensor_type_t type,
        struct sensor_cfg *cfg)
{
    int rc;

    if (type != SENSOR_TYPE_MAGNETIC_FIELD) {
        rc = SYS_EINVAL;
        goto err;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_FLOAT_TRIPLET;

    return (0);
err:
    return (rc);
}
