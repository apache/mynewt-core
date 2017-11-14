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

#if MYNEWT_VAL(SENSOR_CLI)

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
#include "console/console.h"
#include "shell/shell.h"
#include "hal/hal_i2c.h"
#include "os/os_cputime.h"
#include "parse/parse.h"

static int sensor_cmd_exec(int, char **);
static struct shell_cmd shell_sensor_cmd = {
    .sc_cmd = "sensor",
    .sc_cmd_func = sensor_cmd_exec
};

struct os_sem g_sensor_shell_sem;
struct hal_timer g_sensor_shell_timer;
uint32_t sensor_shell_timer_arg = 0xdeadc0de;

struct sensor_poll_data {
    int spd_nsamples;
    int spd_poll_itvl;
    int spd_poll_duration;
    int spd_poll_delay;
};

struct sensor_shell_read_ctx {
    int num_entries;
};

static void
sensor_display_help(void)
{
    console_printf("Possible commands for sensor are:\n");
    console_printf("  list\n");
    console_printf("      list of sensors registered\n");
    console_printf("  read <sensor_name> <type> [-n nsamples] [-i poll_itvl(ms)] [-d poll_duration(ms)]\n");
    console_printf("      read <no_of_samples> from sensor<sensor_name> of type:<type> at preset interval or \n");
    console_printf("      at <poll_interval> rate for <poll_duration>\n");
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
    struct sensor_shell_read_ctx *ctx;
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

    ctx = (struct sensor_shell_read_ctx *) arg;

    ++ctx->num_entries;

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

void
sensor_shell_timer_cb(void *arg)
{
    int timer_arg_val;

    timer_arg_val = *(uint32_t *)arg;
    os_cputime_timer_relative(&g_sensor_shell_timer, timer_arg_val);
    os_sem_release(&g_sensor_shell_sem);
}

/* os cputime timer configuration and initialization */
static void
sensor_shell_config_timer(struct sensor_poll_data *spd)
{
    sensor_shell_timer_arg = spd->spd_poll_itvl * 1000;

    os_cputime_timer_init(&g_sensor_shell_timer, sensor_shell_timer_cb,
                          &sensor_shell_timer_arg);

    os_cputime_timer_relative(&g_sensor_shell_timer, sensor_shell_timer_arg);
}

/* Check for number of samples */
static int
sensor_shell_chk_nsamples(struct sensor_poll_data *spd,
                          struct sensor_shell_read_ctx *ctx)
{
    /* Condition for number of samples */
    if (spd->spd_nsamples && ctx->num_entries >= spd->spd_nsamples) {
        os_cputime_timer_stop(&g_sensor_shell_timer);
        return 0;
    }

    return -1;
}

/* Check for escape sequence */
static int
sensor_shell_chk_escape_seq(void)
{
    char ch;
    int newline;
    int rc;

    ch = 0;
    /* Check for escape sequence */
    rc = console_read(&ch, 1, &newline);
    /* ^C or q or Q gets it out of the sampling loop */
    if (rc || (ch == 3 || ch == 'q' || ch == 'Q')) {
        os_cputime_timer_stop(&g_sensor_shell_timer);
        console_printf("Sensor polling stopped rc:%u\n", rc);
        return 0;
    }

    return -1;
}

/*
 * Incrementing duration based on interval if specified or
 * os_time if interval is not specified and checking duration
 */
static int
sensor_shell_polling_done(struct sensor_poll_data *spd, int64_t *duration,
                          int64_t *start_ts)
{

    if (spd->spd_poll_duration) {
        if (spd->spd_poll_itvl) {
            *duration += spd->spd_poll_itvl * 1000;
        } else {
            if (!*start_ts) {
                *start_ts = os_get_uptime_usec();
            } else {
                *duration = os_get_uptime_usec() - *start_ts;
            }
        }

        if (*duration >= spd->spd_poll_duration * 1000) {
            os_cputime_timer_stop(&g_sensor_shell_timer);
            console_printf("Sensor polling done\n");
            return 0;
        }
    }

    return -1;
}

static int
sensor_cmd_read(char *name, sensor_type_t type, struct sensor_poll_data *spd)
{
    struct sensor *sensor;
    struct sensor_listener listener;
    struct sensor_shell_read_ctx ctx;
    int rc;
    int64_t duration;
    int64_t start_ts;

    /* Look up sensor by name */
    sensor = sensor_mgr_find_next_bydevname(name, NULL);
    if (!sensor) {
        console_printf("Sensor %s not found!\n", name);
    }

    /* Register a listener and then trigger/read a bunch of samples */
    memset(&ctx, 0, sizeof(ctx));

    if (!(type & sensor->s_types)) {
        rc = SYS_EINVAL;
        /* Directly return without trying to unregister */
        console_printf("Read req for wrng type 0x%x from selected sensor: %s\n",
                       (int)type, name);
        return rc;
    }

    listener.sl_sensor_type = type;
    listener.sl_func = sensor_shell_read_listener;
    listener.sl_arg = &ctx;

    rc = sensor_register_listener(sensor, &listener);
    if (rc != 0) {
        return rc;
    }

    /* Initialize the semaphore*/
    os_sem_init(&g_sensor_shell_sem, 0);

    if (spd->spd_poll_itvl) {
        sensor_shell_config_timer(spd);
    }

    start_ts = duration = 0;

    while (1) {
        if (spd->spd_poll_itvl) {
            /*
             * Wait for semaphore from callback,
             * this semaphore should only be considered when a
             * a poll interval is specified
             */
            os_sem_pend(&g_sensor_shell_sem, OS_TIMEOUT_NEVER);
        }

        rc = sensor_read(sensor, type, NULL, NULL, OS_TIMEOUT_NEVER);
        if (rc) {
            console_printf("Cannot read sensor %s\n", name);
            goto err;
        }

        /* Check number of samples if provided */
        if (!sensor_shell_chk_nsamples(spd, &ctx)) {
            break;
        }

        /* Check duration if provided */
        if (!sensor_shell_polling_done(spd, &start_ts, &duration)) {
            break;
        }

        /* Check escape sequence */
        if(!sensor_shell_chk_escape_seq()) {
            break;
        }
    }

err:
    os_sem_release(&g_sensor_shell_sem);

    sensor_unregister_listener(sensor, &listener);

    return rc;
}

int
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

int
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

done:
    return rc;
}

static int
sensor_cmd_exec(int argc, char **argv)
{
    struct sensor_poll_data spd;
    char *subcmd;
    int rc = 0;
    int i;

    if (argc <= 1) {
        sensor_display_help();
        goto done;
    }

    subcmd = argv[1];
    if (!strcmp(subcmd, "list")) {
        sensor_cmd_list_sensors();
    } else if (!strcmp(subcmd, "read")) {
        if (argc < 6) {
            console_printf("Too few arguments: %d\n"
                           "Usage: sensor read <sensor_name> <type>"
                           "[-n nsamples] [-i poll_itvl(ms)] [-d poll_duration(ms)]\n",
                           argc - 2);
            rc = SYS_EINVAL;
            goto done;
        }

        i = 4;
        memset(&spd, 0, sizeof(struct sensor_poll_data));
        if (argv[i] && !strcmp(argv[i], "-n")) {
            spd.spd_nsamples = atoi(argv[++i]);
            i++;
        }
        if (argv[i] && !strcmp(argv[i], "-i")) {
            spd.spd_poll_itvl = atoi(argv[++i]);
            i++;
        }
        if (argv[i] && !strcmp(argv[i], "-d")) {
            spd.spd_poll_duration = atoi(argv[++i]);
            i++;
        }

        rc = sensor_cmd_read(argv[2], (sensor_type_t) strtol(argv[3], NULL, 0), &spd);
        if (rc) {
            goto done;
        }
    } else if (!strcmp(argv[1], "type")) {
        rc = sensor_cmd_display_type(argv);
        if (rc) {
            goto done;
        }
    } else if (!strcmp(argv[1], "notify")) {
        if (argc < 3) {
            console_printf("Too few arguments: %d\n"
                           "Usage: sensor notify <sensor_name> <on/off> <single/double>",
                           argc - 2);
            rc = SYS_EINVAL;
            goto done;
        }

        rc = sensor_cmd_notify(argv[2], !strcmp(argv[3], "on"), argv[4]);
        if (rc) {
            console_printf("Too few arguments: %d\n"
                           "Usage: sensor notify <sensor_name> <on/off> <single/double>",
                           argc - 2);
           goto done;
        }

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
    shell_cmd_register((struct shell_cmd *) &shell_sensor_cmd);

    return (0);
}

#endif
