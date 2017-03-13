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
#include "console/console.h"
#include "shell/shell.h"
#include "hal/hal_i2c.h"

static int sensor_cmd_exec(int, char **);
static struct shell_cmd shell_sensor_cmd = {
    .sc_cmd = "sensor",
    .sc_cmd_func = sensor_cmd_exec
};

static void
sensor_display_help(void)
{
    console_printf("Possible commands for sensor are:\n");
    console_printf("  list\n");
    console_printf("  read\n");
    console_printf("  i2cscan\n");
}

static void
sensor_cmd_display_sensor(struct sensor *sensor)
{
    int type;
    int i;

    console_printf("sensor dev = %s, type = ", sensor->s_dev->od_name);

    for (i = 0; i < 32; i++) {
        type = (0x1 << i) & sensor->s_types;
        if (type) {
            console_printf("0x%x ", type);
        }
    }

    console_printf("\n");
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

struct sensor_shell_read_ctx {
    sensor_type_t type;
    int num_entries;
};

char*
sensor_ftostr(float num, char *fltstr, int len)
{
    memset(fltstr, 0, len);

    snprintf(fltstr, len, "%s%d.%09ld", num < 0.0 ? "-":"", abs((int)num),
             labs((long int)((num - (float)((int)num)) * 1000000000)));
    return fltstr;
}

static int
sensor_shell_read_listener(struct sensor *sensor, void *arg, void *data)
{
    struct sensor_shell_read_ctx *ctx;
    struct sensor_accel_data *sad;
    struct sensor_mag_data *smd;
    struct sensor_light_data *sld;
    struct sensor_euler_data *sed;
    struct sensor_quat_data *sqd;
    struct sensor_color_data *scd;
    int8_t *temperature;
    char tmpstr[13];

    ctx = (struct sensor_shell_read_ctx *) arg;

    ++ctx->num_entries;

    if (ctx->type == SENSOR_TYPE_ACCELEROMETER ||
        ctx->type == SENSOR_TYPE_LINEAR_ACCEL  ||
        ctx->type == SENSOR_TYPE_GRAVITY) {

        sad = (struct sensor_accel_data *) data;
        if (sad->sad_x != SENSOR_ACCEL_DATA_UNUSED) {
            console_printf("x = %s ", sensor_ftostr(sad->sad_x, tmpstr, 13));
        }
        if (sad->sad_y != SENSOR_ACCEL_DATA_UNUSED) {
            console_printf("y = %s ", sensor_ftostr(sad->sad_y, tmpstr, 13));
        }
        if (sad->sad_z != SENSOR_ACCEL_DATA_UNUSED) {
            console_printf("z = %s", sensor_ftostr(sad->sad_z, tmpstr, 13));
        }
        console_printf("\n");
    }

    if (ctx->type == SENSOR_TYPE_MAGNETIC_FIELD) {
        smd = (struct sensor_mag_data *) data;
        if (smd->smd_x != SENSOR_MAG_DATA_UNUSED) {
            console_printf("x = %s ", sensor_ftostr(smd->smd_x, tmpstr, 13));
        }
        if (smd->smd_y != SENSOR_MAG_DATA_UNUSED) {
            console_printf("y = %s ", sensor_ftostr(smd->smd_y, tmpstr, 13));
        }
        if (smd->smd_z != SENSOR_MAG_DATA_UNUSED) {
            console_printf("z = %s ", sensor_ftostr(smd->smd_z, tmpstr, 13));
        }
        console_printf("\n");
    }

    if (ctx->type == SENSOR_TYPE_LIGHT) {
        sld = (struct sensor_light_data *) data;
        if (sld->sld_full != SENSOR_LIGHT_DATA_UNUSED) {
            console_printf("Full = %u, ", sld->sld_full);
        }
        if (sld->sld_ir != SENSOR_LIGHT_DATA_UNUSED) {
            console_printf("IR = %u, ", sld->sld_ir);
        }
        if (sld->sld_lux != SENSOR_LIGHT_DATA_UNUSED) {
            console_printf("Lux = %u, ", (unsigned int)sld->sld_lux);
        }
        console_printf("\n");
    }

    if (ctx->type == SENSOR_TYPE_TEMPERATURE) {
        temperature = (int8_t *) data;
        console_printf("temprature = %d", *temperature);
        console_printf("\n");
    }

    if (ctx->type == SENSOR_TYPE_EULER) {
        sed = (struct sensor_euler_data *) data;
        if (sed->sed_h != SENSOR_EULER_DATA_UNUSED) {
            console_printf("h = %s", sensor_ftostr(sed->sed_h, tmpstr, 13));
        }
        if (sed->sed_r != SENSOR_EULER_DATA_UNUSED) {
            console_printf("r = %s", sensor_ftostr(sed->sed_r, tmpstr, 13));
        }
        if (sed->sed_p != SENSOR_EULER_DATA_UNUSED) {
            console_printf("p = %s", sensor_ftostr(sed->sed_p, tmpstr, 13));
        }
        console_printf("\n");
    }

    if (ctx->type == SENSOR_TYPE_ROTATION_VECTOR) {
        sqd = (struct sensor_quat_data *) data;
        if (sqd->sqd_x != SENSOR_QUAT_DATA_UNUSED) {
            console_printf("x = %s ", sensor_ftostr(sqd->sqd_x, tmpstr, 13));
        }
        if (sqd->sqd_y != SENSOR_QUAT_DATA_UNUSED) {
            console_printf("y = %s ", sensor_ftostr(sqd->sqd_y, tmpstr, 13));
        }
        if (sqd->sqd_z != SENSOR_QUAT_DATA_UNUSED) {
            console_printf("z = %s ", sensor_ftostr(sqd->sqd_z, tmpstr, 13));
        }
        if (sqd->sqd_w != SENSOR_QUAT_DATA_UNUSED) {
            console_printf("w = %s ", sensor_ftostr(sqd->sqd_w, tmpstr, 13));
        }
        console_printf("\n");
    }

    if (ctx->type == SENSOR_TYPE_COLOR) {
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
            console_printf("c = %u, ", scd->scd_c);
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

    return (0);
}

static int
sensor_cmd_read(char *name, sensor_type_t type, int nsamples)
{
    struct sensor *sensor;
    struct sensor_listener listener;
    struct sensor_shell_read_ctx ctx;
    int rc;

    /* Look up sensor by name */
    sensor = sensor_mgr_find_next_bydevname(name, NULL);
    if (!sensor) {
        console_printf("Sensor %s not found!\n", name);
    }

    /* Register a listener and then trigger/read a bunch of samples.
     */
    memset(&ctx, 0, sizeof(ctx));

    if (!(type & sensor->s_types)) {
        rc = SYS_EINVAL;
        /* Directly return without trying to unregister */
        console_printf("Read req for wrng type 0x%x from selected sensor: %s\n",
                       (int)type, name);
        return rc;
    }

    ctx.type = type;

    listener.sl_sensor_type = type;
    listener.sl_func = sensor_shell_read_listener;
    listener.sl_arg = &ctx;

    rc = sensor_register_listener(sensor, &listener);
    if (rc != 0) {
        return rc;
    }

    while (1) {
        rc = sensor_read(sensor, type, NULL, NULL, OS_TIMEOUT_NEVER);
        if (rc) {
            goto err;
        }
        if (ctx.num_entries >= nsamples) {
            break;
        }
    }

    sensor_unregister_listener(sensor, &listener);

    return 0;
err:
    sensor_unregister_listener(sensor, &listener);
    return rc;
}

int
sensor_shell_stol(char *param_val, long min, long max, long *output)
{
    char *endptr;
    long lval;

    lval = strtol(param_val, &endptr, 10); /* Base 10 */
    if (param_val != '\0' && *endptr == '\0' &&
        lval >= min && lval <= max) {
            *output = lval;
    } else {
        return EINVAL;
    }

    return 0;
}

static int
sensor_cmd_i2cscan(int argc, char **argv)
{
    uint8_t addr;
    int32_t timeout;
    uint8_t dev_count = 0;
    long i2cnum;
    int rc;

    timeout = OS_TICKS_PER_SEC / 10;

    if (sensor_shell_stol(argv[2], 0, 0xf, &i2cnum)) {
        console_printf("Invalid i2c interface:%s\n", argv[2]);
        rc = SYS_EINVAL;
        goto err;
    }

    console_printf("Scanning I2C bus %u\n"
                   "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n"
                   "00:          ", (uint8_t)i2cnum);


    /* Scan all valid I2C addresses (0x08..0x77) */
    for (addr = 0x08; addr < 0x78; addr++) {
        rc = hal_i2c_master_probe((uint8_t)i2cnum, addr, timeout);
        /* Print addr header every 16 bytes */
        if (!(addr % 16)) {
            console_printf("\n%02x: ", addr);
        }
        /* Display the addr if a response was received */
        if (!rc) {
            console_printf("%02x ", addr);
            dev_count++;
        } else {
            console_printf("-- ");
        }
        os_time_delay(OS_TICKS_PER_SEC/1000 * 20);
    }
    console_printf("\nFound %u devices on I2C bus %u\n", dev_count, (uint8_t)i2cnum);

    return 0;
err:
    return rc;
}

static int
sensor_cmd_exec(int argc, char **argv)
{
    char *subcmd;
    int rc;

    if (argc <= 1) {
        sensor_display_help();
        rc = 0;
        goto done;
    }

    subcmd = argv[1];
    if (!strcmp(subcmd, "list")) {
        sensor_cmd_list_sensors();
    } else if (!strcmp(subcmd, "read")) {
        if (argc != 5) {
            console_printf("Three arguments required for read: device name, "
                    "sensor type and number of samples, only %d provided.\n",
                    argc - 2);
            rc = SYS_EINVAL;
            goto err;
        }

        rc = sensor_cmd_read(argv[2], (sensor_type_t) strtol(argv[3], NULL, 0), atoi(argv[4]));
        if (rc) {
            goto err;
        }
    } else if (!strcmp(argv[1], "i2cscan")) {
        rc = sensor_cmd_i2cscan(argc, argv);
        if (rc) {
            goto err;
        }
    } else {
        console_printf("Unknown sensor command %s\n", subcmd);
        rc = SYS_EINVAL;
        goto err;
    }

done:
    return (0);
err:
    return (rc);
}


int
sensor_shell_register(void)
{
    shell_cmd_register((struct shell_cmd *) &shell_sensor_cmd);

    return (0);
}

#endif
