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

#if MYNEWT_VAL(SENSOR_CLI)

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

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
#include "console/console.h"
#include "shell/shell.h"
#include "hal/hal_i2c.h"
#include "parse/parse.h"

static int sensor_cmd_exec(int, char **);
static struct shell_cmd shell_sensor_cmd = {
    .sc_cmd = "sensor",
    .sc_cmd_func = sensor_cmd_exec
};

struct sensor_poll_data {
    int spd_nsamples;
    int spd_poll_itvl;
    int spd_poll_duration;

    struct sensor *spd_sensor;
    sensor_type_t spd_sensor_type;

    bool spd_read_in_progress;
    struct os_event spd_read_ev;
    struct hal_timer spd_read_timer;
    uint32_t spd_read_start_ticks;
    uint32_t spd_read_next_msecs_off;
};

static struct sensor_poll_data g_spd;

static void
sensor_display_help(void)
{
    console_printf("Possible commands for sensor are:\n");
    console_printf("  list\n");
    console_printf("      list of sensors registered\n");
    console_printf("  read <sensor_name> <type> [-n nsamples] [-i poll_itvl(ms)] [-d poll_duration(ms)]\n");
    console_printf("      read <no_of_samples> from sensor<sensor_name> of type:<type> at preset interval or \n");
    console_printf("      at <poll_interval> rate for <poll_duration>\n");
    console_printf("  read_stop\n");
    console_printf("      stops polling the sensor\n");
    console_printf("  type <sensor_name>\n");
    console_printf("      types supported by registered sensor\n");
    console_printf("  notify <sensor_name> [on/off] <type>\n");
}

static void
sensor_cmd_display_sensor(struct sensor *sensor)
{
    int i;
    sensor_type_t type;

    console_printf("sensor dev = %s, configured type = ", sensor->s_dev->od_name);
    type = 0x1;

    for (i = 0; i < 32; i++) {
        if (sensor_mgr_match_bytype(sensor, (void *)&type)) {
            console_printf("0x%x ", (unsigned int)type);
        }
        type <<= 1;
    }

    console_printf("\n");
}

static int
sensor_cmd_display_type(char **argv)
{
    int i;
    int rc;
    struct sensor *sensor;
    unsigned int type;

    /* Look up sensor by name */
    sensor = sensor_mgr_find_next_bydevname(argv[2], NULL);
    if (!sensor) {
        console_printf("Sensor %s not found!\n", argv[2]);
        rc = SYS_EINVAL;
        goto err;
    }

    console_printf("sensor dev = %s, \ntype =\n", argv[2]);

    for (i = 0; i < 32; i++) {
        type = (0x1 << i) & sensor->s_types;
        if (!type) {
            continue;
        }

        switch (type) {
            case SENSOR_TYPE_NONE:
                console_printf("    no type: 0x%x\n", type);
                break;
            case SENSOR_TYPE_ACCELEROMETER:
                console_printf("    accelerometer: 0x%x\n", type);
                break;
            case SENSOR_TYPE_MAGNETIC_FIELD:
                console_printf("    magnetic field: 0x%x\n", type);
                break;
            case SENSOR_TYPE_GYROSCOPE:
                console_printf("    gyroscope: 0x%x\n", type);
                break;
            case SENSOR_TYPE_LIGHT:
                console_printf("    light: 0x%x\n", type);
                break;
            case SENSOR_TYPE_TEMPERATURE:
                console_printf("    temperature: 0x%x\n", type);
                break;
            case SENSOR_TYPE_AMBIENT_TEMPERATURE:
                console_printf("    ambient temperature: 0x%x\n", type);
                break;
            case SENSOR_TYPE_PRESSURE:
                console_printf("    pressure: 0x%x\n", type);
                break;
            case SENSOR_TYPE_PROXIMITY:
                console_printf("    proximity: 0x%x\n", type);
                break;
            case SENSOR_TYPE_RELATIVE_HUMIDITY:
                console_printf("    humidity: 0x%x\n", type);
                break;
            case SENSOR_TYPE_ROTATION_VECTOR:
                console_printf("    vector: 0x%x\n", type);
                break;
            case SENSOR_TYPE_ALTITUDE:
                console_printf("    altitude: 0x%x\n", type);
                break;
            case SENSOR_TYPE_WEIGHT:
                console_printf("    weight: 0x%x\n", type);
                break;
            case SENSOR_TYPE_LINEAR_ACCEL:
                console_printf("    accel: 0x%x\n", type);
                break;
            case SENSOR_TYPE_GRAVITY:
                console_printf("    gravity: 0x%x\n", type);
                break;
            case SENSOR_TYPE_EULER:
                console_printf("    euler: 0x%x\n", type);
                break;
            case SENSOR_TYPE_COLOR:
                console_printf("    color: 0x%x\n", type);
                break;
            case SENSOR_TYPE_USER_DEFINED_1:
                console_printf("    user defined 1: 0x%x\n", type);
                break;
            case SENSOR_TYPE_USER_DEFINED_2:
                console_printf("    user defined 2: 0x%x\n", type);
                break;
            case SENSOR_TYPE_USER_DEFINED_3:
                console_printf("    user defined 3: 0x%x\n", type);
                break;
            case SENSOR_TYPE_USER_DEFINED_4:
                console_printf("    user defined 4: 0x%x\n", type);
                break;
            case SENSOR_TYPE_USER_DEFINED_5:
                console_printf("    user defined 5: 0x%x\n", type);
                break;
            case SENSOR_TYPE_USER_DEFINED_6:
                console_printf("    user defined 6: 0x%x\n", type);
                break;
            default:
                console_printf("    unknown type: 0x%x\n", type);
                break;
        }
    }

err:
    return rc;
}

static void
sensor_cmd_list_sensors(void)
{
    struct sensor *sensor;

    sensor = NULL;

    sensor_mgr_lock();

    while (1) {
        sensor = sensor_mgr_find_next_bytype(SENSOR_TYPE_ALL, sensor);
        if (sensor == NULL) {
            break;
        }

        sensor_cmd_display_sensor(sensor);
    }

    sensor_mgr_unlock();
}

/*
 * Convenience API to convert floats to strings,
 * might loose some precision due to rounding
 */
char*
sensor_ftostr(float num, char *fltstr, int len)
{
    memset(fltstr, 0, len);

    snprintf(fltstr, len, "%s%d.%09ld", num < 0.0 ? "-":"", abs((int)num),
             labs((long int)((num - (float)((int)num)) * 1000000000)));
    return fltstr;
}

static int
sensor_shell_read_listener(struct sensor *sensor, void *arg, void *data,
                           sensor_type_t type)
{
    struct sensor_accel_data *sad;
    struct sensor_mag_data *smd;
    struct sensor_light_data *sld;
    struct sensor_euler_data *sed;
    struct sensor_quat_data *sqd;
    struct sensor_color_data *scd;
    struct sensor_temp_data *std;
    struct sensor_press_data *spd;
    struct sensor_humid_data *shd;
    struct sensor_gyro_data *sgd;
    char tmpstr[13];

    console_printf("ts: [ secs: %ld usecs: %d cputime: %u ]\n",
                   (long int)sensor->s_sts.st_ostv.tv_sec,
                   (int)sensor->s_sts.st_ostv.tv_usec,
                   (unsigned int)sensor->s_sts.st_cputime);

    if (type == SENSOR_TYPE_ACCELEROMETER ||
        type == SENSOR_TYPE_LINEAR_ACCEL  ||
        type == SENSOR_TYPE_GRAVITY) {

        sad = (struct sensor_accel_data *) data;
        if (sad->sad_x_is_valid) {
            console_printf("x = %s ", sensor_ftostr(sad->sad_x, tmpstr, 13));
        }
        if (sad->sad_y_is_valid) {
            console_printf("y = %s ", sensor_ftostr(sad->sad_y, tmpstr, 13));
        }
        if (sad->sad_z_is_valid) {
            console_printf("z = %s", sensor_ftostr(sad->sad_z, tmpstr, 13));
        }
        console_printf("\n");
    }

    if (type == SENSOR_TYPE_MAGNETIC_FIELD) {
        smd = (struct sensor_mag_data *) data;
        if (smd->smd_x_is_valid) {
            console_printf("x = %s ", sensor_ftostr(smd->smd_x, tmpstr, 13));
        }
        if (smd->smd_y_is_valid) {
            console_printf("y = %s ", sensor_ftostr(smd->smd_y, tmpstr, 13));
        }
        if (smd->smd_z_is_valid) {
            console_printf("z = %s ", sensor_ftostr(smd->smd_z, tmpstr, 13));
        }
        console_printf("\n");
    }

    if (type == SENSOR_TYPE_GYROSCOPE) {
        sgd = (struct sensor_gyro_data *) data;
        if (sgd->sgd_x_is_valid) {
            console_printf("x = %s ", sensor_ftostr(sgd->sgd_x, tmpstr, 13));
        }
        if (sgd->sgd_y_is_valid) {
            console_printf("y = %s ", sensor_ftostr(sgd->sgd_y, tmpstr, 13));
        }
        if (sgd->sgd_z_is_valid) {
            console_printf("z = %s ", sensor_ftostr(sgd->sgd_z, tmpstr, 13));
        }
        console_printf("\n");
    }

    if (type == SENSOR_TYPE_LIGHT) {
        sld = (struct sensor_light_data *) data;
        if (sld->sld_full_is_valid) {
            console_printf("Full = %u, ", sld->sld_full);
        }
        if (sld->sld_ir_is_valid) {
            console_printf("IR = %u, ", sld->sld_ir);
        }
        if (sld->sld_lux_is_valid) {
            console_printf("Lux = %u, ", (unsigned int)sld->sld_lux);
        }
        console_printf("\n");
    }

    if (type == SENSOR_TYPE_TEMPERATURE      ||
        type == SENSOR_TYPE_AMBIENT_TEMPERATURE) {

        std = (struct sensor_temp_data *) data;
        if (std->std_temp_is_valid) {
            console_printf("temperature = %s Deg C", sensor_ftostr(std->std_temp, tmpstr, 13));
        }
        console_printf("\n");
    }

    if (type == SENSOR_TYPE_EULER) {
        sed = (struct sensor_euler_data *) data;
        if (sed->sed_h_is_valid) {
            console_printf("h = %s", sensor_ftostr(sed->sed_h, tmpstr, 13));
        }
        if (sed->sed_r_is_valid) {
            console_printf("r = %s", sensor_ftostr(sed->sed_r, tmpstr, 13));
        }
        if (sed->sed_p_is_valid) {
            console_printf("p = %s", sensor_ftostr(sed->sed_p, tmpstr, 13));
        }
        console_printf("\n");
    }

    if (type == SENSOR_TYPE_ROTATION_VECTOR) {
        sqd = (struct sensor_quat_data *) data;
        if (sqd->sqd_x_is_valid) {
            console_printf("x = %s ", sensor_ftostr(sqd->sqd_x, tmpstr, 13));
        }
        if (sqd->sqd_y_is_valid) {
            console_printf("y = %s ", sensor_ftostr(sqd->sqd_y, tmpstr, 13));
        }
        if (sqd->sqd_z_is_valid) {
            console_printf("z = %s ", sensor_ftostr(sqd->sqd_z, tmpstr, 13));
        }
        if (sqd->sqd_w_is_valid) {
            console_printf("w = %s ", sensor_ftostr(sqd->sqd_w, tmpstr, 13));
        }
        console_printf("\n");
    }

    if (type == SENSOR_TYPE_COLOR) {
        scd = (struct sensor_color_data *) data;
        if (scd->scd_r_is_valid) {
            console_printf("r = %u, ", scd->scd_r);
        }
        if (scd->scd_g_is_valid) {
            console_printf("g = %u, ", scd->scd_g);
        }
        if (scd->scd_b_is_valid) {
            console_printf("b = %u, ", scd->scd_b);
        }
        if (scd->scd_c_is_valid) {
            console_printf("c = %u, \n", scd->scd_c);
        }
        if (scd->scd_lux_is_valid) {
            console_printf("lux = %u, ", scd->scd_lux);
        }
        if (scd->scd_colortemp_is_valid) {
            console_printf("cct = %uK, ", scd->scd_colortemp);
        }
        if (scd->scd_ir_is_valid) {
            console_printf("ir = %u, \n", scd->scd_ir);
        }
        if (scd->scd_saturation_is_valid) {
            console_printf("sat = %u, ", scd->scd_saturation);
        }
        if (scd->scd_saturation_is_valid) {
            console_printf("sat75 = %u, ", scd->scd_saturation75);
        }
        if (scd->scd_is_sat_is_valid) {
            console_printf(scd->scd_is_sat ? "is saturated, " : "not saturated, ");
        }
        if (scd->scd_cratio_is_valid) {
            console_printf("cRatio = %s, ", sensor_ftostr(scd->scd_cratio, tmpstr, 13));
        }
        if (scd->scd_maxlux_is_valid) {
            console_printf("max lux = %u, ", scd->scd_maxlux);
        }

        console_printf("\n\n");
    }

    if (type == SENSOR_TYPE_PRESSURE) {
        spd = (struct sensor_press_data *) data;
        if (spd->spd_press_is_valid) {
            console_printf("pressure = %s Pa",
                           sensor_ftostr(spd->spd_press, tmpstr, 13));
        }
        console_printf("\n");
    }

    if (type == SENSOR_TYPE_RELATIVE_HUMIDITY) {
        shd = (struct sensor_humid_data *) data;
        if (shd->shd_humid_is_valid) {
            console_printf("relative humidity = %s%%rh",
                           sensor_ftostr(shd->shd_humid, tmpstr, 13));
        }
        console_printf("\n");
    }

    return (0);
}

static void
sensor_shell_read_ev_cb(struct os_event *ev)
{
    uint32_t next_tick;
    int rc;

    rc = sensor_read(g_spd.spd_sensor, g_spd.spd_sensor_type,
                     sensor_shell_read_listener, (void *)SENSOR_IGN_LISTENER,
                     OS_TIMEOUT_NEVER);
    if (rc) {
        console_printf("Cannot read sensor\n");
        goto stop;
    }

    /* If number of samples were provided, check if we should still read */
    if (g_spd.spd_nsamples && (--g_spd.spd_nsamples == 0)) {
        goto stop;
    }

    /* Calculate next read time (it should be in future so skip missed ones) */
    do {
        g_spd.spd_read_next_msecs_off += g_spd.spd_poll_itvl;

        if (g_spd.spd_poll_duration &&
            (g_spd.spd_read_next_msecs_off > g_spd.spd_poll_duration)) {
            goto stop;
        }

        next_tick = g_spd.spd_read_start_ticks +
                    os_cputime_usecs_to_ticks(g_spd.spd_read_next_msecs_off * 1000);
    } while (next_tick <= os_cputime_get32());

    rc = os_cputime_timer_start(&g_spd.spd_read_timer, next_tick);
    if (!rc) {
        return;
    }

    console_printf("Failed to setup read timer\n");
    /* fall through */

stop:
    g_spd.spd_read_in_progress = false;
    os_cputime_timer_stop(&g_spd.spd_read_timer);

    console_printf("Reading done\n");
}

static void
sensor_shell_read_timer_cb(void *arg)
{
    os_eventq_put(os_eventq_dflt_get(), &g_spd.spd_read_ev);
}

static int
sensor_cmd_read(char **argv, int argc)
{
    char *sensor_name;
    sensor_type_t type;
    int i;

    if (argc < 2) {
        /* Need at least sensor name and type */
        console_printf("Too few arguments: %d\n", argc);
        goto usage;
    }

    g_spd.spd_nsamples = 0;
    g_spd.spd_poll_itvl = 0;
    g_spd.spd_poll_duration = 0;

    sensor_name = argv[0];
    type = strtol(argv[1], NULL, 0);

    for (i = 2; i < argc; i++) {
        if (argv[i][0] != '-' || strlen(argv[i]) != 2) {
            console_printf("Invalid parameter '%s'\n", argv[i]);
            goto usage;
        }

        if (argc - i < 2) {
            console_printf("Missing parameter for '%s'\n", argv[i]);
            goto usage;
        }

        switch (argv[i][1]) {
        case 'n':
            g_spd.spd_nsamples = atoi(argv[++i]);
            break;
        case 'i':
            g_spd.spd_poll_itvl = atoi(argv[++i]);
            break;
        case 'd':
            g_spd.spd_poll_duration = atoi(argv[++i]);
            break;
        default:
            console_printf("Unknown option '%s'\n", argv[i]);
            goto usage;
        }
    }

    if (!g_spd.spd_nsamples && !g_spd.spd_poll_itvl && !g_spd.spd_poll_duration) {
        /* Just read single sample by default */
        g_spd.spd_nsamples = 1;
    }

    if ((g_spd.spd_nsamples > 1) && !g_spd.spd_poll_itvl) {
        console_printf("Need to specify poll interval if num_samples > 0\n");
        goto usage;
    }

    /* Look up sensor by name */
    g_spd.spd_sensor = sensor_mgr_find_next_bydevname(sensor_name, NULL);
    if (!g_spd.spd_sensor) {
        console_printf("Sensor %s not found!\n", sensor_name);
        return SYS_ENOENT;
    }

    if (!(type & g_spd.spd_sensor->s_types)) {
        /* Directly return without trying to unregister */
        console_printf("Read request for wrong type 0x%x from selected sensor: %s\n",
                       (int)type, sensor_name);
        return SYS_EINVAL;
    }

    g_spd.spd_sensor_type = type;

    /* Start 1st read immediately */
    g_spd.spd_read_next_msecs_off = 0;
    g_spd.spd_read_start_ticks = os_cputime_get32();
    sensor_shell_read_timer_cb(NULL);

    return 0;

usage:
    console_printf("Usage: sensor read <sensor_name> <type> "
                   "[-n num_samples] [-i poll_interval(ms)] [-d poll_duration(ms)]\n");

    return SYS_EINVAL;
}

static int
sensor_one_tap_notif(struct sensor *sensor, void *data,
                     sensor_event_type_t type)
{

    console_printf("Single tap happend\n");

    return 0;
};

static struct sensor_notifier one_tap = {
    .sn_sensor_event_type = SENSOR_EVENT_TYPE_SINGLE_TAP,
    .sn_func = sensor_one_tap_notif,
    .sn_arg = NULL,
};

static int
sensor_double_tap_notif(struct sensor *sensor, void *data,
                        sensor_event_type_t type)
{
    console_printf("Double tap happend\n");

    return 0;
};

static struct sensor_notifier double_tap = {
    .sn_sensor_event_type = SENSOR_EVENT_TYPE_DOUBLE_TAP,
    .sn_func = sensor_double_tap_notif,
    .sn_arg = NULL,
};


static int
sensor_wakeup_notif(struct sensor *sensor, void *data,
                  sensor_event_type_t type)
{
    console_printf("wakeup happend\n");

    return 0;
};

static struct sensor_notifier wakeup = {
    .sn_sensor_event_type = SENSOR_EVENT_TYPE_WAKEUP,
    .sn_func = sensor_wakeup_notif,
    .sn_arg = NULL,
};

static int
sensor_free_fall_notif(struct sensor *sensor, void *data,
                  sensor_event_type_t type)
{
    console_printf("free fall happend\n");

    return 0;
};

static struct sensor_notifier free_fall = {
    .sn_sensor_event_type = SENSOR_EVENT_TYPE_FREE_FALL,
    .sn_func = sensor_free_fall_notif,
    .sn_arg = NULL,
};

static int
sensor_orient_change_notif(struct sensor *sensor, void *data,
                  sensor_event_type_t type)
{
    console_printf("orient change happend\n");

    return 0;
};

static struct sensor_notifier orient_change = {
    .sn_sensor_event_type = SENSOR_EVENT_TYPE_ORIENT_CHANGE,
    .sn_func = sensor_orient_change_notif,
    .sn_arg = NULL,
};

static int
sensor_sleep_notif(struct sensor *sensor, void *data,
                  sensor_event_type_t type)
{
    console_printf("sleep happend\n");

    return 0;
};

static struct sensor_notifier sensor_sleep = {
    .sn_sensor_event_type = SENSOR_EVENT_TYPE_SLEEP,
    .sn_func = sensor_sleep_notif,
    .sn_arg = NULL,
};

static int
sensor_orient_xl_change_notif(struct sensor *sensor, void *data,
                  sensor_event_type_t type)
{
    console_printf("orient x l change happend\n");

    return 0;
};

static struct sensor_notifier orient_xl_change = {
    .sn_sensor_event_type = SENSOR_EVENT_TYPE_ORIENT_X_L_CHANGE,
    .sn_func = sensor_orient_xl_change_notif,
    .sn_arg = NULL,
};

static int
sensor_orient_yl_change_notif(struct sensor *sensor, void *data,
                  sensor_event_type_t type)
{
    console_printf("orient y l change happend\n");

    return 0;
};

static struct sensor_notifier orient_yl_change = {
    .sn_sensor_event_type = SENSOR_EVENT_TYPE_ORIENT_Y_L_CHANGE,
    .sn_func = sensor_orient_yl_change_notif,
    .sn_arg = NULL,
};

static int
sensor_orient_zl_change_notif(struct sensor *sensor, void *data,
                  sensor_event_type_t type)
{
    console_printf("orient z l change happend\n");

    return 0;
};

static struct sensor_notifier orient_zl_change = {
    .sn_sensor_event_type = SENSOR_EVENT_TYPE_ORIENT_Z_L_CHANGE,
    .sn_func = sensor_orient_zl_change_notif,
    .sn_arg = NULL,
};

static int
sensor_orient_xh_change_notif(struct sensor *sensor, void *data,
                  sensor_event_type_t type)
{
    console_printf("orient x h change happend\n");

    return 0;
};

static struct sensor_notifier orient_xh_change = {
    .sn_sensor_event_type = SENSOR_EVENT_TYPE_ORIENT_X_H_CHANGE,
    .sn_func = sensor_orient_xh_change_notif,
    .sn_arg = NULL,
};

static int
sensor_orient_yh_change_notif(struct sensor *sensor, void *data,
                  sensor_event_type_t type)
{
    console_printf("orient y h change happend\n");

    return 0;
};

static struct sensor_notifier orient_yh_change = {
    .sn_sensor_event_type = SENSOR_EVENT_TYPE_ORIENT_Y_H_CHANGE,
    .sn_func = sensor_orient_yh_change_notif,
    .sn_arg = NULL,
};

static int
sensor_orient_zh_change_notif(struct sensor *sensor, void *data,
                  sensor_event_type_t type)
{
    console_printf("orient z h change happend\n");

    return 0;
};

static struct sensor_notifier orient_zh_change = {
    .sn_sensor_event_type = SENSOR_EVENT_TYPE_ORIENT_Z_H_CHANGE,
    .sn_func = sensor_orient_zh_change_notif,
    .sn_arg = NULL,
};




static int
sensor_cmd_notify(char *name, bool on, char *type_string)
{
    struct sensor *sensor;
    int type = 0;
    int rc = 0;

    /* Look up sensor by name */
    sensor = sensor_mgr_find_next_bydevname(name, NULL);
    if (!sensor) {
        console_printf("Sensor %s not found!\n", name);
    }

    if (!strcmp(type_string, "single")) {
        type = SENSOR_EVENT_TYPE_SINGLE_TAP;
    } else if (!strcmp(type_string, "double")) {
        type = SENSOR_EVENT_TYPE_DOUBLE_TAP;
    } else if (!strcmp(type_string, "wakeup")) {
        type = SENSOR_EVENT_TYPE_WAKEUP;
    } else if (!strcmp(type_string, "freefall")) {
        type = SENSOR_EVENT_TYPE_FREE_FALL;
    } else if (!strcmp(type_string, "orient")) {
        type = SENSOR_EVENT_TYPE_ORIENT_CHANGE;
    } else if (!strcmp(type_string, "sleep")) {
        type = SENSOR_EVENT_TYPE_SLEEP;
    } else if (!strcmp(type_string, "orient_xl")) {
        type = SENSOR_EVENT_TYPE_ORIENT_X_L_CHANGE;
    } else if (!strcmp(type_string, "orient_yl")) {
        type = SENSOR_EVENT_TYPE_ORIENT_Y_L_CHANGE;
    } else if (!strcmp(type_string, "orient_zl")) {
        type = SENSOR_EVENT_TYPE_ORIENT_Z_L_CHANGE;
    } else if (!strcmp(type_string, "orient_xh")) {
        type = SENSOR_EVENT_TYPE_ORIENT_X_H_CHANGE;
    } else if (!strcmp(type_string, "orient_yh")) {
        type = SENSOR_EVENT_TYPE_ORIENT_Y_H_CHANGE;
    } else if (!strcmp(type_string, "orient_zh")) {
        type = SENSOR_EVENT_TYPE_ORIENT_Z_H_CHANGE;
    } else {
        return 1;
    }

    if (!on) {
        if (type == SENSOR_EVENT_TYPE_SINGLE_TAP) {
            rc = sensor_unregister_notifier(sensor, &one_tap);
            if (rc) {
                console_printf("Could not unregister single tap\n");
                goto done;
            }
        }
        if (type == SENSOR_EVENT_TYPE_DOUBLE_TAP) {
            rc = sensor_unregister_notifier(sensor, &double_tap);
            if (rc) {
                 console_printf("Could not unregister double tap\n");
                 goto done;
            }
        }
        if (type == SENSOR_EVENT_TYPE_WAKEUP) {
            rc = sensor_unregister_notifier(sensor, &wakeup);
            if (rc) {
                 console_printf("Could not unregister wakeup\n");
                 goto done;
            }
        }
        if (type == SENSOR_EVENT_TYPE_FREE_FALL) {
            rc = sensor_unregister_notifier(sensor, &free_fall);
            if (rc) {
                 console_printf("Could not unregister free fall\n");
                 goto done;
            }
        }
        if (type == SENSOR_EVENT_TYPE_ORIENT_CHANGE) {
            rc = sensor_unregister_notifier(sensor, &orient_change);
            if (rc) {
                 console_printf("Could not unregister orient change\n");
                 goto done;
            }
        }
        if (type == SENSOR_EVENT_TYPE_SLEEP) {
            rc = sensor_unregister_notifier(sensor, &sensor_sleep);
            if (rc) {
                 console_printf("Could not unregister sleep\n");
                 goto done;
            }
        }
        if (type == SENSOR_EVENT_TYPE_ORIENT_X_H_CHANGE) {
            rc = sensor_unregister_notifier(sensor, &orient_xh_change);
            if (rc) {
                 console_printf("Could not unregister orient change pos x\n");
                 goto done;
            }
        }
        if (type == SENSOR_EVENT_TYPE_ORIENT_Y_H_CHANGE) {
            rc = sensor_unregister_notifier(sensor, &orient_yh_change);
            if (rc) {
                 console_printf("Could not unregister orient change pos y\n");
                 goto done;
            }
        }
        if (type == SENSOR_EVENT_TYPE_ORIENT_Z_H_CHANGE) {
            rc = sensor_unregister_notifier(sensor, &orient_zh_change);
            if (rc) {
                 console_printf("Could not unregister orient change pos z\n");
                 goto done;
            }
        }
        if (type == SENSOR_EVENT_TYPE_ORIENT_X_L_CHANGE) {
            rc = sensor_unregister_notifier(sensor, &orient_xl_change);
            if (rc) {
                 console_printf("Could not unregister orient change neg x\n");
                 goto done;
            }
        }
        if (type == SENSOR_EVENT_TYPE_ORIENT_Y_L_CHANGE) {
            rc = sensor_unregister_notifier(sensor, &orient_yl_change);
            if (rc) {
                 console_printf("Could not unregister orient change neg y\n");
                 goto done;
            }
        }
        if (type == SENSOR_EVENT_TYPE_ORIENT_Z_L_CHANGE) {
            rc = sensor_unregister_notifier(sensor, &orient_zl_change);
            if (rc) {
                 console_printf("Could not unregister orient change neg z\n");
                 goto done;
            }
        }

        goto done;
    }

    if (type == SENSOR_EVENT_TYPE_SINGLE_TAP) {
        rc = sensor_register_notifier(sensor, &one_tap);
        if (rc) {
            console_printf("Could not register single tap\n");
            goto done;
        }
    }

    if (type == SENSOR_EVENT_TYPE_DOUBLE_TAP) {
        rc = sensor_register_notifier(sensor, &double_tap);
        if (rc) {
             console_printf("Could not register double tap\n");
             goto done;
        }
    }

    if (type == SENSOR_EVENT_TYPE_WAKEUP) {
        rc = sensor_register_notifier(sensor, &wakeup);
        if (rc) {
             console_printf("Could not register wakeup\n");
             goto done;
        }
    }

    if (type == SENSOR_EVENT_TYPE_FREE_FALL) {
        rc = sensor_register_notifier(sensor, &free_fall);
        if (rc) {
             console_printf("Could not register free fall\n");
             goto done;
        }
    }

    if (type == SENSOR_EVENT_TYPE_ORIENT_CHANGE) {
        rc = sensor_register_notifier(sensor, &orient_change);
        if (rc) {
            console_printf("Could not register orient change\n");
            goto done;
        }
    }

    if (type == SENSOR_EVENT_TYPE_SLEEP) {
        rc = sensor_register_notifier(sensor, &sensor_sleep);
        if (rc) {
             console_printf("Could not register sleep\n");
             goto done;
        }
    }
    if (type == SENSOR_EVENT_TYPE_ORIENT_X_H_CHANGE) {
        rc = sensor_register_notifier(sensor, &orient_xh_change);
        if (rc) {
             console_printf("Could not register orient change pos x\n");
             goto done;
        }
    }
    if (type == SENSOR_EVENT_TYPE_ORIENT_Y_H_CHANGE) {
        rc = sensor_register_notifier(sensor, &orient_yh_change);
        if (rc) {
             console_printf("Could not register orient change pos y\n");
             goto done;
        }
    }
    if (type == SENSOR_EVENT_TYPE_ORIENT_Z_H_CHANGE) {
        rc = sensor_register_notifier(sensor, &orient_zh_change);
        if (rc) {
             console_printf("Could not register orient change pos z\n");
             goto done;
        }
    }
    if (type == SENSOR_EVENT_TYPE_ORIENT_X_L_CHANGE) {
        rc = sensor_register_notifier(sensor, &orient_xl_change);
        if (rc) {
             console_printf("Could not register orient change neg x\n");
             goto done;
        }
    }
    if (type == SENSOR_EVENT_TYPE_ORIENT_Y_L_CHANGE) {
        rc = sensor_register_notifier(sensor, &orient_yl_change);
        if (rc) {
             console_printf("Could not register orient change neg y\n");
             goto done;
        }
    }
    if (type == SENSOR_EVENT_TYPE_ORIENT_Z_L_CHANGE) {
        rc = sensor_register_notifier(sensor, &orient_zl_change);
        if (rc) {
             console_printf("Could not register orient change neg z\n");
             goto done;
        }
    }

done:
    return rc;
}

static int
sensor_cmd_exec(int argc, char **argv)
{
    char *subcmd;
    int rc = 0;

    if (argc <= 1) {
        sensor_display_help();
        goto done;
    }

    subcmd = argv[1];
    if (!strcmp(subcmd, "list")) {
        sensor_cmd_list_sensors();
    } else if (!strcmp(subcmd, "read")) {
        if (g_spd.spd_read_in_progress == true) {
            console_printf("Read already in progress\n");
            rc = SYS_EINVAL;
            goto done;
        }

        rc = sensor_cmd_read(argv + 2, argc - 2);
        if (rc) {
            goto done;
        }

        g_spd.spd_read_in_progress = true;
    } else if (!strcmp(argv[1], "type")) {
        rc = sensor_cmd_display_type(argv);
        if (rc) {
            goto done;
        }
    } else if (!strcmp(argv[1], "notify")) {
        if (argc < 3) {
            console_printf("Too few arguments: %d\n"
                           "Usage: sensor notify <sensor_name> <on/off> "
                           "<single/double/wakeup/freefall/orient/sleep/orient_xl/orient_yl/orient_zl/orient_xh/orient_yh/orient_zh>",
                           argc - 2);
            rc = SYS_EINVAL;
            goto done;
        }

        rc = sensor_cmd_notify(argv[2], !strcmp(argv[3], "on"), argv[4]);
        if (rc) {
            console_printf("Too few arguments: %d\n"
                           "Usage: sensor notify <sensor_name> <on/off> "
                           "<single/double/wakeup/freefall/orient/sleep/orient_xl/orient_yl/orient_zl/orient_xh/orient_yh/orient_zh>",
                           argc - 2);
           goto done;
        }
    } else if (!strcmp(argv[1], "read_stop")) {
        if (!g_spd.spd_read_in_progress) {
            console_printf("No read in progress\n");
            rc = SYS_EINVAL;
            goto done;
        }

        console_printf("Reading stopped\n");
        os_cputime_timer_stop(&g_spd.spd_read_timer);
        g_spd.spd_read_in_progress = false;
    } else {
        console_printf("Unknown sensor command %s\n", subcmd);
        rc = SYS_EINVAL;
        goto done;
    }

done:
    return (rc);
}


int
sensor_shell_register(void)
{
    memset(&g_spd, 0, sizeof(g_spd));
    g_spd.spd_read_ev.ev_cb  = sensor_shell_read_ev_cb;
    os_cputime_timer_init(&g_spd.spd_read_timer, sensor_shell_read_timer_cb, NULL);

    shell_cmd_register((struct shell_cmd *) &shell_sensor_cmd);

    return (0);
}

#endif
