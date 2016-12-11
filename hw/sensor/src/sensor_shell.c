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
#include <errno.h>
#include <assert.h>

#include "defs/error.h"

#include "os/os.h"

#include "sensor/sensor.h"
#include "console/console.h"
#include "shell/shell.h"

static int sensor_cmd_exec(int, char **);
static const struct shell_cmd shell_sensor_cmd = {
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
