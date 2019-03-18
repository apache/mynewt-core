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

#include "os/mynewt.h"
#include "sensor/sensor.h"
#include "sensor_test.h"

struct stcpe_error_rec {
    struct sensor *sensor;
    void *arg;
    int status;
    sensor_type_t type;
};

#define STCPE_MAX_ERROR_RECS    16

static struct stcpe_error_rec stcpe_error_recs[STCPE_MAX_ERROR_RECS];
static int stcpe_num_error_recs;

/** Specifies the return code of subsequent sensor reads. */
static int stcpe_sensor_read_status;

/**
 * Sensor read error callback.  Adds an error record to the global
 * `stcpe_error_recs` array.
 */
static void
stcpe_sensor_err(struct sensor *sensor, void *arg, int status)
{
    struct stcpe_error_rec *rec;

    TEST_ASSERT_FATAL(stcpe_num_error_recs < STCPE_MAX_ERROR_RECS);

    rec = &stcpe_error_recs[stcpe_num_error_recs++];
    rec->sensor = sensor;
    rec->arg = arg;
    rec->status = status;
}

/**
 * Sensor read function.
 */
static int
stcpe_sensor_read(struct sensor *sensor, sensor_type_t type,
                  sensor_data_func_t data_func, void *arg, uint32_t timeout)
{
    return stcpe_sensor_read_status;
}

TEST_CASE_SELF(sensor_test_case_poll_err)
{
    static struct sensor_driver driver = {
        .sd_read = stcpe_sensor_read,
    };

    struct sensor sn;
    int rc;

    /***
     * Register the sensor and an error callback.  Arbitrarily specify
     * `&stcpe_sensor_read_status` as the callback argument, just so we can
     * test that the argument is properly passed.
     */

    rc = sensor_init(&sn, NULL);
    TEST_ASSERT_FATAL(rc == 0);

    rc = sensor_set_driver(&sn,
                           SENSOR_TYPE_ACCELEROMETER | SENSOR_TYPE_LIGHT,
                           &driver);
    TEST_ASSERT_FATAL(rc == 0);

    sensor_set_type_mask(&sn, SENSOR_TYPE_ALL);

    rc = sensor_mgr_register(&sn);
    TEST_ASSERT_FATAL(rc == 0);

    rc = sensor_register_err_func(&sn, stcpe_sensor_err,
                                  &stcpe_sensor_read_status);
    TEST_ASSERT_FATAL(rc == 0);

    /*** Successful read; ensure no error. */

    stcpe_sensor_read_status = 0;
    rc = sensor_read(&sn, SENSOR_TYPE_ACCELEROMETER, NULL, NULL,
                     OS_TIMEOUT_NEVER);
    TEST_ASSERT_FATAL(rc == 0);
    TEST_ASSERT_FATAL(stcpe_num_error_recs == 0);

    /*** Three failed reads; ensure three error record. */

    stcpe_sensor_read_status = 1;
    rc = sensor_read(&sn, SENSOR_TYPE_ACCELEROMETER, NULL, NULL,
                     OS_TIMEOUT_NEVER);
    TEST_ASSERT_FATAL(rc == 1);

    stcpe_sensor_read_status = 2;
    rc = sensor_read(&sn, SENSOR_TYPE_ACCELEROMETER, NULL, NULL,
                     OS_TIMEOUT_NEVER);
    TEST_ASSERT_FATAL(rc == 2);

    stcpe_sensor_read_status = 3;
    rc = sensor_read(&sn, SENSOR_TYPE_ACCELEROMETER, NULL, NULL,
                     OS_TIMEOUT_NEVER);
    TEST_ASSERT_FATAL(rc == 3);

    TEST_ASSERT_FATAL(stcpe_num_error_recs == 3);

    TEST_ASSERT_FATAL(stcpe_error_recs[0].sensor == &sn);
    TEST_ASSERT_FATAL(stcpe_error_recs[0].arg == &stcpe_sensor_read_status);
    TEST_ASSERT_FATAL(stcpe_error_recs[0].status == 1);
    TEST_ASSERT_FATAL(stcpe_error_recs[1].sensor == &sn);
    TEST_ASSERT_FATAL(stcpe_error_recs[1].arg == &stcpe_sensor_read_status);
    TEST_ASSERT_FATAL(stcpe_error_recs[1].status == 2);
    TEST_ASSERT_FATAL(stcpe_error_recs[2].sensor == &sn);
    TEST_ASSERT_FATAL(stcpe_error_recs[2].arg == &stcpe_sensor_read_status);
    TEST_ASSERT_FATAL(stcpe_error_recs[2].status == 3);
}
