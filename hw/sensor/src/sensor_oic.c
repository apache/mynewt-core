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

#include "syscfg/syscfg.h"

#if MYNEWT_VAL(SENSOR_OIC)

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "defs/error.h"

#include "os/os.h"

#include "sensor/sensor.h"
#include "sensor/accel.h"
#include "sensor/mag.h"
#include "sensor/light.h"
#include "sensor/quat.h"
#include "sensor/euler.h"
#include "sensor/color.h"
#include "sensor/temperature.h"
#include "sensor/pressure.h"
#include "sensor/humidity.h"
#include "sensor/gyro.h"

/* OIC */
#include <oic/oc_rep.h>
#include <oic/oc_ri.h>
#include <oic/oc_api.h>

static const char g_s_oic_dn[] = "x.mynewt.snsr.";

static int
sensor_oic_encode(struct sensor* sensor, void *arg, void *databuf,
                  sensor_type_t type)
{

    switch(type) {
        /* Gyroscope supported */
        case SENSOR_TYPE_GYROSCOPE:

            if (((struct sensor_gyro_data *)(databuf))->sgd_x_is_valid) {
                oc_rep_set_double(root, x,
                    ((struct sensor_gyro_data *)(databuf))->sgd_x);
            } else {
                goto err;
            }
            if (((struct sensor_gyro_data *)(databuf))->sgd_y_is_valid) {
                oc_rep_set_double(root, y,
                    ((struct sensor_gyro_data *)(databuf))->sgd_y);
            } else {
                goto err;
            }
            if (((struct sensor_gyro_data *)(databuf))->sgd_z_is_valid) {
                oc_rep_set_double(root, z,
                    ((struct sensor_gyro_data *)(databuf))->sgd_z);
            } else {
                goto err;
            }
            break;

        /* Accelerometer functionality supported */
        case SENSOR_TYPE_ACCELEROMETER:
        /* Linear Accelerometer (Without Gravity) */
        case SENSOR_TYPE_LINEAR_ACCEL:
        /* Gravity Sensor */
        case SENSOR_TYPE_GRAVITY:

            if (((struct sensor_accel_data *)(databuf))->sad_x_is_valid) {
                oc_rep_set_double(root, x,
                    ((struct sensor_accel_data *)(databuf))->sad_x);
            } else {
                goto err;
            }
            if (((struct sensor_accel_data *)(databuf))->sad_y_is_valid) {
                oc_rep_set_double(root, y,
                    ((struct sensor_accel_data *)(databuf))->sad_y);
            } else {
                goto err;
            }
            if (((struct sensor_accel_data *)(databuf))->sad_z_is_valid) {
                oc_rep_set_double(root, z,
                    ((struct sensor_accel_data *)(databuf))->sad_z);
            } else {
                goto err;
            }
            break;

        /* Magnetic field supported */
        case SENSOR_TYPE_MAGNETIC_FIELD:
            if (((struct sensor_mag_data *)(databuf))->smd_x_is_valid) {
                oc_rep_set_double(root, x,
                    ((struct sensor_mag_data *)(databuf))->smd_x);
            } else {
                goto err;
            }
            if (((struct sensor_mag_data *)(databuf))->smd_y_is_valid) {
                oc_rep_set_double(root, y,
                    ((struct sensor_mag_data *)(databuf))->smd_y);
            } else {
                goto err;
            }
            if (((struct sensor_mag_data *)(databuf))->smd_z_is_valid) {
                oc_rep_set_double(root, z,
                    ((struct sensor_mag_data *)(databuf))->smd_z);
            } else {
                goto err;
            }
            break;
        /* Light supported */
        case SENSOR_TYPE_LIGHT:
            if (((struct sensor_light_data *)(databuf))->sld_ir_is_valid) {
                oc_rep_set_double(root, ir,
                    ((struct sensor_light_data *)(databuf))->sld_ir);
            } else {
                goto err;
            }
            if (((struct sensor_light_data *)(databuf))->sld_full_is_valid) {
                oc_rep_set_double(root, full,
                    ((struct sensor_light_data *)(databuf))->sld_full);
            } else {
                goto err;
            }
            if (((struct sensor_light_data *)(databuf))->sld_lux_is_valid) {
                oc_rep_set_double(root, lux,
                    ((struct sensor_light_data *)(databuf))->sld_lux);
            } else {
                goto err;
            }
            break;

        /* Temperature supported */
        case SENSOR_TYPE_TEMPERATURE:
            if (((struct sensor_temp_data *)(databuf))->std_temp_is_valid) {
                oc_rep_set_double(root, temp,
                    ((struct sensor_temp_data *)(databuf))->std_temp);
            }
            break;

        /* Ambient temperature supported */
        case SENSOR_TYPE_AMBIENT_TEMPERATURE:
            if (((struct sensor_temp_data *)(databuf))->std_temp_is_valid) {
                oc_rep_set_double(root, temp,
                    ((struct sensor_temp_data *)(databuf))->std_temp);
            }
            break;

        /* Pressure sensor supported */
        case SENSOR_TYPE_PRESSURE:
            if (((struct sensor_press_data *)(databuf))->spd_press_is_valid) {
                oc_rep_set_double(root, press,
                    ((struct sensor_press_data *)(databuf))->spd_press);
            }
            break;

        /* Relative humidity supported */
        case SENSOR_TYPE_RELATIVE_HUMIDITY:
            if (((struct sensor_humid_data *)(databuf))->shd_humid_is_valid) {
                oc_rep_set_double(root, humid,
                    ((struct sensor_humid_data *)(databuf))->shd_humid);
            }
            break;

        /* Rotation vector (quaternion) supported */
        case SENSOR_TYPE_ROTATION_VECTOR:
            if (((struct sensor_quat_data *)(databuf))->sqd_x_is_valid) {
                oc_rep_set_double(root, x,
                    ((struct sensor_quat_data *)(databuf))->sqd_x);
            } else {
                goto err;
            }
            if (((struct sensor_quat_data *)(databuf))->sqd_y_is_valid) {
                oc_rep_set_double(root, y,
                    ((struct sensor_quat_data *)(databuf))->sqd_y);
            } else {
                goto err;
            }
            if (((struct sensor_quat_data *)(databuf))->sqd_z_is_valid) {
                oc_rep_set_double(root, z,
                    ((struct sensor_quat_data *)(databuf))->sqd_z);
            } else {
                goto err;
            }
            if (((struct sensor_quat_data *)(databuf))->sqd_w_is_valid) {
                oc_rep_set_double(root, w,
                    ((struct sensor_quat_data *)(databuf))->sqd_w);
            } else {
                goto err;
            }
            break;

        /* Euler Orientation Sensor */
        case SENSOR_TYPE_EULER:
            if (((struct sensor_euler_data *)(databuf))->sed_h_is_valid) {
                oc_rep_set_double(root, h,
                    ((struct sensor_euler_data *)(databuf))->sed_h);
            } else {
                goto err;
            }
            if (((struct sensor_euler_data *)(databuf))->sed_r_is_valid) {
                oc_rep_set_double(root, r,
                    ((struct sensor_euler_data *)(databuf))->sed_r);
            } else {
                goto err;
            }
            if (((struct sensor_euler_data *)(databuf))->sed_p_is_valid) {
                oc_rep_set_double(root, p,
                    ((struct sensor_euler_data *)(databuf))->sed_p);
            } else {
                goto err;
            }
            break;

        /* Color Sensor */
        case SENSOR_TYPE_COLOR:
            if (((struct sensor_color_data *)(databuf))->scd_r_is_valid) {
                oc_rep_set_uint(root, r,
                    ((struct sensor_color_data *)(databuf))->scd_r);
            } else {
                goto err;
            }
            if (((struct sensor_color_data *)(databuf))->scd_g_is_valid) {
                oc_rep_set_uint(root, g,
                    ((struct sensor_color_data *)(databuf))->scd_g);
            } else {
                goto err;
            }
            if (((struct sensor_color_data *)(databuf))->scd_b_is_valid) {
                oc_rep_set_uint(root, b,
                    ((struct sensor_color_data *)(databuf))->scd_b);
            } else {
                goto err;
            }
            if (((struct sensor_color_data *)(databuf))->scd_lux_is_valid) {
                oc_rep_set_uint(root, lux,
                    ((struct sensor_color_data *)(databuf))->scd_lux);
            } else {
                goto err;
            }
            if (((struct sensor_color_data *)(databuf))->scd_colortemp_is_valid) {
                oc_rep_set_uint(root, colortemp,
                    ((struct sensor_color_data *)(databuf))->scd_colortemp);
            } else {
                goto err;
            }
            if (((struct sensor_color_data *)(databuf))->scd_saturation_is_valid) {
                oc_rep_set_uint(root, saturation,
                    ((struct sensor_color_data *)(databuf))->scd_saturation);
            } else {
                goto err;
            }
            if (((struct sensor_color_data *)(databuf))->scd_saturation75_is_valid) {
                oc_rep_set_uint(root, saturation75,
                    ((struct sensor_color_data *)(databuf))->scd_saturation75);
            } else {
                goto err;
            }
            if (((struct sensor_color_data *)(databuf))->scd_is_sat_is_valid) {
                oc_rep_set_uint(root, is_sat,
                    ((struct sensor_color_data *)(databuf))->scd_is_sat);
            } else {
                goto err;
            }
            if (((struct sensor_color_data *)(databuf))->scd_cratio_is_valid) {
                oc_rep_set_double(root, cratio,
                    ((struct sensor_color_data *)(databuf))->scd_cratio);
            } else {
                goto err;
            }
            if (((struct sensor_color_data *)(databuf))->scd_maxlux_is_valid) {
                oc_rep_set_uint(root, maxlux,
                    ((struct sensor_color_data *)(databuf))->scd_maxlux);
            } else {
                goto err;
            }
            if (((struct sensor_color_data *)(databuf))->scd_ir_is_valid) {
                oc_rep_set_uint(root, ir,
                    ((struct sensor_color_data *)(databuf))->scd_ir);
            } else {
                goto err;
            }
            break;

        /* Support for these sensors is currently not there, hence
         * encoding would fail for them
         */

        /* Proximity sensor supported */
        case SENSOR_TYPE_PROXIMITY:
        /* Altitude Supported */
        case SENSOR_TYPE_ALTITUDE:
        /* Weight Supported */
        case SENSOR_TYPE_WEIGHT:
        /* User defined sensor type 1 */
        case SENSOR_TYPE_USER_DEFINED_1:
        /* User defined sensor type 2 */
        case SENSOR_TYPE_USER_DEFINED_2:
        /* User defined sensor type 3 */
        case SENSOR_TYPE_USER_DEFINED_3:
        /* User defined sensor type 4 */
        case SENSOR_TYPE_USER_DEFINED_4:
        /* User defined sensor type 5 */
        case SENSOR_TYPE_USER_DEFINED_5:
        /* User defined sensor type 6 */
        case SENSOR_TYPE_USER_DEFINED_6:

        /* None */
        case SENSOR_TYPE_NONE:
        default:
            goto err;
    }

    oc_rep_set_uint(root, ts_secs, (long int)sensor->s_sts.st_ostv.tv_sec);
    oc_rep_set_int(root, ts_usecs, (int)sensor->s_sts.st_ostv.tv_usec);
    oc_rep_set_uint(root, ts_cputime, (unsigned int)sensor->s_sts.st_cputime);

    return 0;
err:
    return SYS_EINVAL;
}

static int
sensor_typename_to_type(char *typename, sensor_type_t *type,
                        struct sensor *sensor)
{
    if (!strcmp(typename, "acc")) {
        /* Accelerometer functionality supported */
        *type = SENSOR_TYPE_ACCELEROMETER;
    } else if (!strcmp(typename, "mag")) {
        /* Magnetic field supported */
        *type = SENSOR_TYPE_MAGNETIC_FIELD;
    } else if (!strcmp(typename, "gyr")) {
        /* Gyroscope supported */
        *type = SENSOR_TYPE_GYROSCOPE;
    } else if (!strcmp(typename, "lt")) {
        /* Light supported */
        *type = SENSOR_TYPE_LIGHT;
    } else if (!strcmp(typename, "tmp")) {
        /* Temperature supported */
        *type = SENSOR_TYPE_TEMPERATURE;
    } else if (!strcmp(typename, "ambtmp")) {
        /* Ambient temperature supported */
        *type = SENSOR_TYPE_AMBIENT_TEMPERATURE;
    } else if (!strcmp(typename, "psr")) {
        /* Pressure sensor supported */
        *type = SENSOR_TYPE_PRESSURE;
    } else if (!strcmp(typename, "prox")) {
        /* Proximity sensor supported */
        *type = SENSOR_TYPE_PROXIMITY;
    } else if (!strcmp(typename, "rhmty")) {
        /* Relative humidity supported */
        *type = SENSOR_TYPE_RELATIVE_HUMIDITY;
    } else if (!strcmp(typename, "quat")) {
        /* Rotation vector (quaternion)) supported */
        *type = SENSOR_TYPE_ROTATION_VECTOR;
    } else if (!strcmp(typename, "alt")) {
        /* Altitude Supported */
        *type = SENSOR_TYPE_ALTITUDE;
    } else if (!strcmp(typename, "wt")) {
        /* Weight Supported */
        *type = SENSOR_TYPE_WEIGHT;
    } else if (!strcmp(typename, "lacc")) {
        /* Linear Accelerometer (Without Gravity)) */
        *type = SENSOR_TYPE_LINEAR_ACCEL;
    } else if (!strcmp(typename, "grav")) {
        /* Gravity Sensor */
        *type = SENSOR_TYPE_GRAVITY;
    } else if (!strcmp(typename, "eul")) {
        /* Euler Orientation Sensor */
        *type = SENSOR_TYPE_EULER;
    } else if (!strcmp(typename, "col")) {
        /* Color Sensor */
        *type = SENSOR_TYPE_COLOR;
    } else if (!strcmp(typename, "udef1")) {
        /* User defined sensor type 1 */
        *type = SENSOR_TYPE_USER_DEFINED_1;
    } else if (!strcmp(typename, "udef2")) {
        /* User defined sensor type 2 */
        *type = SENSOR_TYPE_USER_DEFINED_2;
    } else if (!strcmp(typename, "udef3")) {
        /* User defined sensor type 3 */
        *type = SENSOR_TYPE_USER_DEFINED_3;
    } else if (!strcmp(typename, "udef4")) {
        /* User defined sensor type 4 */
        *type = SENSOR_TYPE_USER_DEFINED_4;
    } else if (!strcmp(typename, "udef5")) {
        /* User defined sensor type 5 */
        *type = SENSOR_TYPE_USER_DEFINED_5;
    } else if (!strcmp(typename, "udef6")) {
        /* User defined sensor type 6 */
        *type = SENSOR_TYPE_USER_DEFINED_6;
    } else {
        goto err;
    }

    return SYS_EOK;
err:
    return SYS_EINVAL;
}

static int
sensor_type_to_typename(char **typename, sensor_type_t type,
                        struct sensor *sensor)
{

    if (!sensor_mgr_match_bytype(sensor, (void *)&type)) {
        goto err;
    }

    switch(type) {
        /* Accelerometer functionality supported */
        case SENSOR_TYPE_ACCELEROMETER:
            *typename =  "acc";
            break;
        /* Magnetic field supported */
        case SENSOR_TYPE_MAGNETIC_FIELD:
            *typename = "mag";
            break;
        /* Gyroscope supported */
        case SENSOR_TYPE_GYROSCOPE:
            *typename = "gyr";
            break;
        /* Light supported */
        case SENSOR_TYPE_LIGHT:
            *typename = "lt";
            break;
        /* Temperature supported */
        case SENSOR_TYPE_TEMPERATURE:
            *typename = "tmp";
            break;
        /* Ambient temperature supported */
        case SENSOR_TYPE_AMBIENT_TEMPERATURE:
            *typename = "ambtmp";
            break;
        /* Pressure sensor supported */
        case SENSOR_TYPE_PRESSURE:
            *typename = "psr";
            break;
        /* Proximity sensor supported */
        case SENSOR_TYPE_PROXIMITY:
            *typename = "prox";
            break;
        /* Relative humidity supported */
        case SENSOR_TYPE_RELATIVE_HUMIDITY:
            *typename = "rhmty";
            break;
        /* Rotation vector (quaternion)) supported */
        case SENSOR_TYPE_ROTATION_VECTOR:
            *typename = "quat";
            break;
        /* Altitude Supported */
        case SENSOR_TYPE_ALTITUDE:
            *typename = "alt";
            break;
        /* Weight Supported */
        case SENSOR_TYPE_WEIGHT:
            *typename = "wt";
            break;
        /* Linear Accelerometer (Without Gravity)) */
        case SENSOR_TYPE_LINEAR_ACCEL:
            *typename = "lacc";
            break;
        /* Gravity Sensor */
        case SENSOR_TYPE_GRAVITY:
            *typename = "grav";
            break;
        /* Euler Orientation Sensor */
        case SENSOR_TYPE_EULER:
            *typename = "eul";
            break;
        /* Color Sensor */
        case SENSOR_TYPE_COLOR:
            *typename = "col";
            break;
        /* User defined sensor type 1 */
        case SENSOR_TYPE_USER_DEFINED_1:
            *typename = "udef1";
            break;
        /* User defined sensor type 2 */
        case SENSOR_TYPE_USER_DEFINED_2:
            *typename = "udef2";
            break;
        /* User defined sensor type 3 */
        case SENSOR_TYPE_USER_DEFINED_3:
            *typename = "udef3";
            break;
        /* User defined sensor type 4 */
        case SENSOR_TYPE_USER_DEFINED_4:
            *typename = "udef4";
            break;
        /* User defined sensor type 5 */
        case SENSOR_TYPE_USER_DEFINED_5:
            *typename = "udef5";
            break;
        /* User defined sensor type 6 */
        case SENSOR_TYPE_USER_DEFINED_6:
            *typename = "udef6";
            break;
        default:
            goto err;
    }

    return SYS_EOK;
err:
    return SYS_EINVAL;
}

static void
sensor_oic_get_data(oc_request_t *request, oc_interface_mask_t interface)
{
    int rc;
    struct sensor *sensor;
    struct sensor_listener listener;
    char *devname;
    char *typename;
    sensor_type_t type;
    char tmpstr[COAP_MAX_URI] = {0};
    const char s[2] = "/";

    memcpy(tmpstr, (char *)&(request->resource->uri.os_str[1]),
           request->resource->uri.os_sz - 1);

    /* Parse the sensor device name from the uri  */
    devname = strtok(tmpstr, s);

    typename = strtok(NULL, s);

    /* Look up sensor by name */
    sensor = sensor_mgr_find_next_bydevname(devname, NULL);
    if (!sensor) {
        rc = SYS_EINVAL;
        goto err;
    }

    if (memcmp(g_s_oic_dn, request->resource->types.oa_arr.s,
               sizeof(g_s_oic_dn) - 1)) {
        goto err;
    }

    oc_rep_start_root_object();

    switch (interface) {
    case OC_IF_BASELINE:
        oc_process_baseline_interface(request->resource);
    case OC_IF_R:
        /* Register a listener and then trigger/read a bunch of samples */
        memset(&listener, 0, sizeof(listener));

        typename =
            &(request->resource->types.oa_arr.s[sizeof(g_s_oic_dn) - 1]);
        rc = sensor_typename_to_type(typename, &type, sensor);
        if (rc) {
            /* Type either not supported by sensor or not found */
            goto err;
        }

        listener.sl_sensor_type = type;
        listener.sl_func = sensor_oic_encode;

        rc = sensor_register_listener(sensor, &listener);
        if (rc) {
            goto err;
        }

        /* Sensor read results.  Every time a sensor is read, all of its
         * listeners are called by default.  Specify NULL as a callback,
         * because we just want to run all the listeners.
         */
        rc = sensor_read(sensor, listener.sl_sensor_type, NULL, NULL,
                         OS_TIMEOUT_NEVER);
        if (rc) {
            goto err;
        }

        break;
    default:
        break;
    }

    sensor_unregister_listener(sensor, &listener);
    oc_rep_end_root_object();
    oc_send_response(request, OC_STATUS_OK);
    return;
err:
    sensor_unregister_listener(sensor, &listener);
    oc_send_response(request, OC_STATUS_NOT_FOUND);
}

void
sensor_oic_init(void)
{
    oc_resource_t *res;
    struct sensor *sensor;
    char tmpstr[COAP_MAX_URI];
    sensor_type_t type;
    char *typename;
    int i;
    int rc;

    sensor = NULL;

    while (1) {

        sensor_mgr_lock();

        sensor = sensor_mgr_find_next_bytype(SENSOR_TYPE_ALL, sensor);

        sensor_mgr_unlock();

        if (sensor == NULL) {
            /* Sensor not found */
            break;
        }

        i = 0;
        typename = NULL;

        /* Iterate through N types of sensors */
        while (i < 32) {
            type = (1 << i);
            if (sensor_mgr_match_bytype(sensor, &type)) {
                rc = sensor_type_to_typename(&typename, type, sensor);
                if (rc) {
                    break;
                }

                memset(tmpstr, 0, sizeof(tmpstr));
                snprintf(tmpstr, sizeof(tmpstr), "/%s/%s", sensor->s_dev->od_name, typename);

                res = oc_new_resource(tmpstr, 1, 0);

                memset(tmpstr, 0, sizeof(tmpstr));
                snprintf(tmpstr, sizeof(tmpstr), "%s%s", g_s_oic_dn, typename);
                oc_resource_bind_resource_type(res, tmpstr);
                oc_resource_bind_resource_interface(res, OC_IF_R);
                oc_resource_set_default_interface(res, OC_IF_R);

                oc_resource_set_discoverable(res);
                oc_resource_set_periodic_observable_ms(res, MYNEWT_VAL(SENSOR_OIC_OBS_RATE));
                oc_resource_set_request_handler(res, OC_GET,
                                                sensor_oic_get_data);
                oc_add_resource(res);
            }
            i++;
        }
    }
}

#endif
