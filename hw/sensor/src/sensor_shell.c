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
#include "console/console.h"
#include "shell/shell.h"

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
}

static void
sensor_cmd_display_sensor(struct sensor *sensor)
{
    console_printf("sensor dev = %s, type = %lld\n", sensor->s_dev->od_name,
            sensor->s_types);
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
    char tmpstr[13];

    ctx = (struct sensor_shell_read_ctx *) arg;

    ++ctx->num_entries;

    if (ctx->type == SENSOR_TYPE_ACCELEROMETER) {
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

    return (0);
}

static void
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

    ctx.type = type;

    listener.sl_sensor_type = type;
    listener.sl_func = sensor_shell_read_listener;
    listener.sl_arg = &ctx;

    rc = sensor_register_listener(sensor, &listener);
    if (rc != 0) {
        goto err;
    }

    while (1) {
        rc = sensor_read(sensor, type, NULL, NULL, OS_TIMEOUT_NEVER);
        if (ctx.num_entries >= nsamples) {
            break;
        }
    }

    sensor_unregister_listener(sensor, &listener);

err:
    return;
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
        sensor_cmd_read(argv[2], (sensor_type_t) atoi(argv[3]), atoi(argv[4]));
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
