/**
 * Copyright (c) 2017-2025 Bosch Sensortec GmbH. All rights reserved.
 *
 * BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @file       bmp5_shell.c
 * @date       2025-08-13
 * @version    v1.0.0
 *
 */

#include <string.h>
#include <errno.h>
#include "os/mynewt.h"
#include "console/console.h"
#include "sensor/temperature.h"
#include "sensor/pressure.h"
#include "bmp5/bmp5.h"
#include "bmp5_priv.h"

#if MYNEWT_VAL(BMP5_CLI)

#include "shell/shell.h"
#include "parse/parse.h"

struct stream_read_context {
    int count;
};

static int bmp5_shell_cmd(int argc, char **argv);

static struct shell_cmd bmp5_shell_cmd_struct = { .sc_cmd = "bmp5",
                                                  .sc_cmd_func = bmp5_shell_cmd };

static int
bmp5_shell_err_too_many_args(char *cmd_name)
{
    console_printf("Error: too many arguments for command \"%s\"\n", cmd_name);
    return SYS_EINVAL;
}

static int
bmp5_shell_err_unknown_arg(char *cmd_name)
{
    console_printf("Error: unknown argument \"%s\"\n", cmd_name);
    return SYS_EINVAL;
}

static int
bmp5_shell_err_invalid_arg(char *cmd_name)
{
    console_printf("Error: invalid argument \"%s\"\n", cmd_name);
    return SYS_EINVAL;
}

static int
bmp5_shell_help(void)
{
    console_printf("%s cmd [flags...]\n", bmp5_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\tpoll_read    [n_samples] [report_interval_ms]\n");
    console_printf("\tstream_read    [n_samples]\n");
    console_printf("\tchipid\n");
    console_printf("\tdump\n");
    console_printf("\ttest\n");

    return 0;
}

static int
bmp5_shell_cmd_read_chipid(int argc, char **argv)
{
    int rc;
    uint8_t chipid;
    struct os_dev *dev;
    struct bmp5 *bmp5;

    dev = os_dev_open(MYNEWT_VAL(BMP5_SHELL_DEV_NAME), OS_TIMEOUT_NEVER, NULL);
    if (dev == NULL) {
        console_printf("failed to open %s device\n", dev->od_name);
        return SYS_ENODEV;
    }

    bmp5 = (struct bmp5 *)dev;

    rc = bmp5_get_chip_id(bmp5, &chipid);
    if (rc) {
        goto err;
    }

    console_printf("CHIP_ID:0x%02X\n", chipid);

    return 0;
err:
    return rc;
}

int
bmp5_stream_read_cb(struct sensor *sensors, void *arg, void *data, sensor_type_t sensortype)
{
    char buffer_pressure[20];
    char buffer_temperature[20];
    struct stream_read_context *ctx;
    struct sensor_temp_data *std;
    struct sensor_press_data *spd;

    if (sensortype & SENSOR_TYPE_PRESSURE) {
        spd = (struct sensor_press_data *)data;
        sensor_ftostr(spd->spd_press, buffer_pressure, sizeof(buffer_pressure));
        console_printf("pressure = %s \n", buffer_pressure);
    }

    if (sensortype & SENSOR_TYPE_TEMPERATURE) {
        std = (struct sensor_temp_data *)data;
        sensor_ftostr(std->std_temp, buffer_temperature, sizeof(buffer_temperature));
        console_printf("temperature = %s \n", buffer_temperature);
    }

    ctx = (struct stream_read_context *)arg;
    if (--ctx->count <= 0) {
        return 1;
    }

    return 0;
}

static int
bmp5_shell_cmd_stream_read(int argc, char **argv)
{
    uint16_t samples = 1;
    uint16_t val;
    int rc;
    struct os_dev *dev;
    struct bmp5 *bmp5;
    struct stream_read_context ctx;

    if (argc > 3) {
        return bmp5_shell_err_too_many_args(argv[1]);
    }

    /* Check if more than one sample requested */
    if (argc == 3) {
        val = parse_ll_bounds(argv[2], 1, UINT16_MAX, &rc);
        if (rc) {
            return bmp5_shell_err_invalid_arg(argv[2]);
        }
        samples = val;
    }

    dev = os_dev_open(MYNEWT_VAL(BMP5_SHELL_DEV_NAME), OS_TIMEOUT_NEVER, NULL);
    if (dev == NULL) {
        console_printf("failed to open %s device\n", dev->od_name);
        return SYS_ENODEV;
    }

    bmp5 = (struct bmp5 *)dev;
    ctx.count = samples;

    console_printf("bmp5_shell_cmd_streamread!\n");

    return bmp5_stream_read(&(bmp5->sensor),
                            SENSOR_TYPE_PRESSURE | SENSOR_TYPE_TEMPERATURE,
                            bmp5_stream_read_cb, &ctx, 0);
}

static int
bmp5_shell_cmd_poll_read(int argc, char **argv)
{
    uint16_t samples = 1;
    uint16_t report_interval = 1;
    uint16_t val;
    int rc;
    struct os_dev *dev;
    struct bmp5 *bmp5;
    struct stream_read_context ctx;

    if (argc > 4) {
        return bmp5_shell_err_too_many_args(argv[1]);
    }

    /* Check if more than one sample requested */
    if (argc == 4) {
        val = parse_ll_bounds(argv[2], 1, UINT16_MAX, &rc);
        if (rc) {
            return bmp5_shell_err_invalid_arg(argv[2]);
        }
        samples = val;

        val = parse_ll_bounds(argv[3], 1, UINT16_MAX, &rc);
        if (rc) {
            return bmp5_shell_err_invalid_arg(argv[3]);
        }
        report_interval = val;
    }

    dev = os_dev_open(MYNEWT_VAL(BMP5_SHELL_DEV_NAME), OS_TIMEOUT_NEVER, NULL);
    if (dev == NULL) {
        console_printf("failed to open %s device\n", dev->od_name);
        return SYS_ENODEV;
    }

    bmp5 = (struct bmp5 *)dev;
    ctx.count = samples;

    console_printf("bmp5_shell_cmd_poll_read!\n");

    if ((samples > 0) && (report_interval)) {
        while (samples != 0) {
            bmp5_poll_read(&(bmp5->sensor),
                           SENSOR_TYPE_PRESSURE | SENSOR_TYPE_TEMPERATURE,
                           bmp5_stream_read_cb, &ctx, 0);
            samples--;
            os_time_delay((report_interval * OS_TICKS_PER_SEC) / 1000 + 1);
        }
    }
    return 0;
}

static int
bmp5_shell_cmd_dump(int argc, char **argv)
{
    int rc = 0;
    struct os_dev *dev;
    struct bmp5 *bmp5;

    if (argc > 2) {
        return bmp5_shell_err_too_many_args(argv[1]);
    }
    dev = os_dev_open(MYNEWT_VAL(BMP5_SHELL_DEV_NAME), OS_TIMEOUT_NEVER, NULL);
    if (dev == NULL) {
        console_printf("failed to open %s device\n", dev->od_name);
        return SYS_ENODEV;
    }

    bmp5 = (struct bmp5 *)dev;

    rc = bmp5_dump(bmp5);

    return rc;
}

static int
bmp5_shell_cmd_test(int argc, char **argv)
{
    int rc;
    int result;
    struct os_dev *dev;
    struct bmp5 *bmp5;

    dev = os_dev_open(MYNEWT_VAL(BMP5_SHELL_DEV_NAME), OS_TIMEOUT_NEVER, NULL);
    if (dev == NULL) {
        console_printf("failed to open %s device\n", dev->od_name);
        return SYS_ENODEV;
    }

    bmp5 = (struct bmp5 *)dev;

    rc = bmp5_run_self_test(bmp5, &result);
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
bmp5_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return bmp5_shell_help();
    }

    /* Read command (get stream read sample) */
    if (argc > 1 && strcmp(argv[1], "stream_read") == 0) {
        return bmp5_shell_cmd_stream_read(argc, argv);
    }

    /* Read command (get poll read sample) */
    if (argc > 1 && strcmp(argv[1], "poll_read") == 0) {
        return bmp5_shell_cmd_poll_read(argc, argv);
    }

    /* Chip ID */
    if (argc > 1 && strcmp(argv[1], "chipid") == 0) {
        return bmp5_shell_cmd_read_chipid(argc, argv);
    }

    /* Dump */
    if (argc > 1 && strcmp(argv[1], "dump") == 0) {
        return bmp5_shell_cmd_dump(argc, argv);
    }

    /* Test */
    if (argc > 1 && strcmp(argv[1], "test") == 0) {
        return bmp5_shell_cmd_test(argc, argv);
    }

    return bmp5_shell_err_unknown_arg(argv[1]);
}

int
bmp5_shell_init(void)
{
    int rc;

    rc = shell_cmd_register(&bmp5_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

#endif
