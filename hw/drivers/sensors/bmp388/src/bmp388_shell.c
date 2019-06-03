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
/**\mainpage
* Copyright (C) 2017 - 2019 Bosch Sensortec GmbH
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*
* Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* Neither the name of the copyright holder nor the names of the
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER
* OR CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
* OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
* ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
*
* The information provided is believed to be accurate and reliable.
* The copyright holder assumes no responsibility
* for the consequences of use
* of such information nor for any infringement of patents or
* other rights of third parties which may result from its use.
* No license is granted by implication or otherwise under any patent or
* patent rights of the copyright holder.
*
* File   bmp388_shell.c
* Date   10 May 2019
* Version   1.0.2
*
*/


#include <string.h>
#include <errno.h>
#include "os/mynewt.h"
#include "console/console.h"
#include "sensor/temperature.h"
#include "sensor/pressure.h"
#include "bmp388/bmp388.h"
#include "bmp388_priv.h"

#if MYNEWT_VAL(BMP388_CLI)

#include "shell/shell.h"
#include "parse/parse.h"

struct stream_read_context {
    uint32_t count;
};

static int bmp388_shell_cmd(int argc, char **argv);

static struct shell_cmd bmp388_shell_cmd_struct = {
    .sc_cmd = "bmp388",
    .sc_cmd_func = bmp388_shell_cmd
};


static int
bmp388_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n",
                cmd_name);
    return EINVAL;
}

static int
bmp388_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n",
                cmd_name);
    return EINVAL;
}

static int
bmp388_shell_err_invalid_arg(char *cmd_name)
{
    console_printf("Error: invalid argument \"%s\"\n",
                cmd_name);
    return EINVAL;
}

static int
bmp388_shell_help(void)
{
    console_printf("%s cmd [flags...]\n", bmp388_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\tpoll_read    [n_samples] [report_interval_ms]\n");
    console_printf("\tstream_read    [n_samples]\n");
    console_printf("\tchipid\n");
    console_printf("\tdump\n");
    console_printf("\ttest\n");

    return 0;
}

static int
bmp388_shell_cmd_read_chipid(int argc, char **argv)
{
    int rc;
    uint8_t chipid;
    struct os_dev * dev;
    struct bmp388 *bmp388;
    struct sensor_itf *itf;
    dev = os_dev_open(MYNEWT_VAL(BMP388_SHELL_DEV_NAME), OS_TIMEOUT_NEVER, NULL);
    if (dev == NULL) {
        console_printf("failed to open bmp388_0 device\n");
        return ENODEV;
    }

    bmp388 = (struct bmp388 *)dev;
    itf = SENSOR_GET_ITF(&(bmp388->sensor));

    rc = bmp388_get_chip_id(itf, &chipid);
    if (rc) {
        goto err;
    }

    console_printf("CHIP_ID:0x%02X\n", chipid);

    return 0;
err:
    return rc;
}


int bmp388_stream_read_cb(struct sensor * sensors, void *arg, void *data,
            sensor_type_t sensortype)
{
    char buffer_pressure[20];
    char buffer_temperature[20];
    struct stream_read_context * ctx;
    struct sensor_temp_data *std;
    struct sensor_press_data *spd;

    if (sensortype & SENSOR_TYPE_PRESSURE)
    {
        spd = (struct sensor_press_data *)data;
        sensor_ftostr(spd->spd_press, buffer_pressure, sizeof(buffer_pressure));
        console_printf("pressure = %s \n",
                buffer_pressure);
    }

    if (sensortype & SENSOR_TYPE_AMBIENT_TEMPERATURE)
    {
        std = (struct sensor_temp_data *)data;
        sensor_ftostr(std->std_temp, buffer_temperature, sizeof(buffer_temperature));
        console_printf("temperature = %s \n",
                buffer_temperature);
    }




    ctx = (struct stream_read_context *)arg;
    ctx->count--;

    return 0;
}

static int
bmp388_shell_cmd_stream_read(int argc, char **argv)
{
    uint16_t samples = 1;
    uint16_t val;
    int rc;
    struct os_dev * dev;
    struct bmp388 *bmp388;
    struct stream_read_context ctx;

    if (argc > 3) {
        return bmp388_shell_err_too_many_args(argv[1]);
    }

    /* Check if more than one sample requested */
    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 1, UINT16_MAX, &rc);
        if (rc) {
            return bmp388_shell_err_invalid_arg(argv[2]);
        }
        samples = val;
    }

    dev = os_dev_open(MYNEWT_VAL(BMP388_SHELL_DEV_NAME), OS_TIMEOUT_NEVER, NULL);
    if (dev == NULL) {
        console_printf("failed to open bmp388_0 device\n");
        return ENODEV;
    }

    bmp388 = (struct bmp388 *)dev;
    ctx.count = samples;

    console_printf("bmp388_shell_cmd_streamread!\n");

    return bmp388_stream_read(&(bmp388->sensor),
                                SENSOR_TYPE_PRESSURE | SENSOR_TYPE_AMBIENT_TEMPERATURE,
                                bmp388_stream_read_cb,
                                &ctx,
                                0);
}

static int
bmp388_shell_cmd_poll_read(int argc, char **argv)
{
    uint16_t samples = 1;
    uint16_t report_interval = 1;
    uint16_t val;
    int rc;
    struct os_dev * dev;
    struct bmp388 *bmp388;
    struct stream_read_context ctx;

    if (argc > 4) {
        return bmp388_shell_err_too_many_args(argv[1]);
    }

    /* Check if more than one sample requested */
    if (argc == 4) {
        val = parse_ll_bounds(argv[2], 1, UINT16_MAX, &rc);
        if (rc) {
            return bmp388_shell_err_invalid_arg(argv[2]);
        }
        samples = val;

        val = parse_ll_bounds(argv[3], 1, UINT16_MAX, &rc);
        if (rc) {
            return bmp388_shell_err_invalid_arg(argv[3]);
        }
        report_interval = val;

    }

    dev = os_dev_open(MYNEWT_VAL(BMP388_SHELL_DEV_NAME), OS_TIMEOUT_NEVER, NULL);
    if (dev == NULL) {
        console_printf("failed to open bmp388_0 device\n");
        return ENODEV;
    }

    bmp388 = (struct bmp388 *)dev;
    ctx.count = samples;

    console_printf("bmp388_shell_cmd_poll_read!\n");

    if ((samples > 0) && (report_interval))
    {
        while (samples != 0)
        {
            bmp388_poll_read(&(bmp388->sensor),
                                SENSOR_TYPE_PRESSURE | SENSOR_TYPE_AMBIENT_TEMPERATURE,
                                bmp388_stream_read_cb,
                                &ctx,
                                0);
            samples--;
            os_time_delay((report_interval * OS_TICKS_PER_SEC) / 1000 + 1);
        }
    }
    return 0;
}


static int
bmp388_shell_cmd_dump(int argc, char **argv)
{
    int rc = 0;
    struct os_dev * dev;
    struct bmp388 *bmp388;
    struct sensor_itf *itf;
    if (argc > 2) {
        return bmp388_shell_err_too_many_args(argv[1]);
    }
    dev = os_dev_open(MYNEWT_VAL(BMP388_SHELL_DEV_NAME), OS_TIMEOUT_NEVER, NULL);
    if (dev == NULL) {
        console_printf("failed to open bmp388_0 device\n");
        return ENODEV;
    }

    bmp388 = (struct bmp388 *)dev;
    itf = SENSOR_GET_ITF(&(bmp388->sensor));


    rc = bmp388_dump(itf);

    return rc;
}

static int
bmp388_shell_cmd_test(int argc, char **argv)
{
    int rc;
    int result;
    struct os_dev * dev;
    struct bmp388 *bmp388;
    struct sensor_itf *itf;
    dev = os_dev_open(MYNEWT_VAL(BMP388_SHELL_DEV_NAME), OS_TIMEOUT_NEVER, NULL);
    if (dev == NULL) {
        console_printf("failed to open bmp388_0 device\n");
        return ENODEV;
    }

    bmp388 = (struct bmp388 *)dev;
    itf = SENSOR_GET_ITF(&(bmp388->sensor));

    rc = bmp388_run_self_test(itf, &result);
    if (rc) {
        goto err;
    }

    if (result) {
        console_printf("SELF TEST: FAILED\n");
    } else {
        console_printf("SELF TEST: PASSED\n");
    }

    return 0;
err:
    return rc;
}

static int
bmp388_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return bmp388_shell_help();
    }

    /* Read command (get stream read sample) */
    if (argc > 1 && strcmp(argv[1], "stream_read") == 0) {
        return bmp388_shell_cmd_stream_read(argc, argv);
    }

    /* Read command (get poll read sample) */
    if (argc > 1 && strcmp(argv[1], "poll_read") == 0) {
        return bmp388_shell_cmd_poll_read(argc, argv);
    }


    /* Chip ID */
    if (argc > 1 && strcmp(argv[1], "chipid") == 0) {
        return bmp388_shell_cmd_read_chipid(argc, argv);
    }

    /* Dump */
    if (argc > 1 && strcmp(argv[1], "dump") == 0) {
        return bmp388_shell_cmd_dump(argc, argv);
    }


    /* Test */
    if (argc > 1 && strcmp(argv[1], "test") == 0) {
        return bmp388_shell_cmd_test(argc, argv);
    }

    return bmp388_shell_err_unknown_arg(argv[1]);
}

int
bmp388_shell_init(void)
{
    int rc;

    rc = shell_cmd_register(&bmp388_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

#endif
