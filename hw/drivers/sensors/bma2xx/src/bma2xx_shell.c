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

#include <errno.h>

#include "syscfg/syscfg.h"
#include "bma2xx/bma2xx.h"
#include "bma2xx_priv.h"
#include "console/console.h"
#include "shell/shell.h"

#if MYNEWT_VAL(BMA2XX_CLI)

struct self_test_mode {
    const char * name;
    float hmult;
    float lmult;
};

static const struct self_test_mode self_test_modes[] = {
    {
        .name  = "default",
        .hmult = 1.0,
        .lmult = 0.0,
    },
    {
        .name  = "strict",
        .hmult = 2.0,
        .lmult = 0.5,
    },
};

static int
bma2xx_self_test_cmd(struct bma2xx * bma2xx,
                     int argc,
                     char * argv[])
{
    const struct self_test_mode * self_test_mode;
    uint8_t i;
    bool self_test_fail;
    int rc;

    if (argc != 1) {
        return EINVAL;
    }

    self_test_mode = NULL;
    for (i = 0; i < sizeof(self_test_modes) /
                    sizeof(*self_test_modes); i++) {
        if (strcmp(self_test_modes[i].name, argv[0]) == 0) {
            self_test_mode = self_test_modes + i;
        }
    }

    if (self_test_mode == NULL) {
        return EINVAL;
    }

    rc = bma2xx_self_test(bma2xx,
                          self_test_mode->hmult,
                          self_test_mode->lmult,
                          &self_test_fail);
    if (rc != 0) {
        return rc;
    }

    if (self_test_fail) {
        console_printf("self test failed\n");
    } else {
        console_printf("self test passed\n");
    }

    return 0;
}

struct offset_comp_target {
    const char * name;
    enum bma2xx_offset_comp_target target;
};

static const struct offset_comp_target offset_comp_targets[] = {
    {
        .name   = "0g",
        .target = BMA2XX_OFFSET_COMP_TARGET_0_G,
    },
    {
        .name   = "-1g",
        .target = BMA2XX_OFFSET_COMP_TARGET_NEG_1_G,
    },
    {
        .name   = "+1g",
        .target = BMA2XX_OFFSET_COMP_TARGET_POS_1_G,
    },
};

static int
bma2xx_offset_compensation_cmd(struct bma2xx * bma2xx,
                               int argc,
                               char * argv[])
{
    const struct offset_comp_target * target_x;
    const struct offset_comp_target * target_y;
    const struct offset_comp_target * target_z;
    uint8_t i;

    if (argc != 3) {
        return EINVAL;
    }

    target_x = NULL;
    for (i = 0; i < sizeof(offset_comp_targets) /
                    sizeof(*offset_comp_targets); i++) {
        if (strcmp(offset_comp_targets[i].name, argv[0]) == 0) {
            target_x = offset_comp_targets + i;
        }
    }
    target_y = NULL;
    for (i = 0; i < sizeof(offset_comp_targets) /
                    sizeof(*offset_comp_targets); i++) {
        if (strcmp(offset_comp_targets[i].name, argv[1]) == 0) {
            target_y = offset_comp_targets + i;
        }
    }
    target_z = NULL;
    for (i = 0; i < sizeof(offset_comp_targets) /
                    sizeof(*offset_comp_targets); i++) {
        if (strcmp(offset_comp_targets[i].name, argv[2]) == 0) {
            target_z = offset_comp_targets + i;
        }
    }

    if (target_x == NULL) {
        return EINVAL;
    }
    if (target_y == NULL) {
        return EINVAL;
    }
    if (target_z == NULL) {
        return EINVAL;
    }

    return bma2xx_offset_compensation(bma2xx,
                                      target_x->target,
                                      target_y->target,
                                      target_z->target);
}

static int
bma2xx_query_offsets_cmd(struct bma2xx * bma2xx,
                         int argc,
                         char * argv[])
{
    int rc;
    float offset_x_g;
    float offset_y_g;
    float offset_z_g;
    char buffer_x[20];
    char buffer_y[20];
    char buffer_z[20];

    if (argc != 0) {
        return EINVAL;
    }

    rc = bma2xx_query_offsets(bma2xx,
                              &offset_x_g,
                              &offset_y_g,
                              &offset_z_g);
    if (rc != 0) {
        return rc;
    }

    sensor_ftostr(offset_x_g, buffer_x, sizeof(buffer_x));
    sensor_ftostr(offset_y_g, buffer_y, sizeof(buffer_y));
    sensor_ftostr(offset_z_g, buffer_z, sizeof(buffer_z));

    console_printf("offset x = %s offset y = %s offset z = %s\n",
                   buffer_x,
                   buffer_y,
                   buffer_z);

    return 0;
}

static int
bma2xx_write_offsets_cmd(struct bma2xx * bma2xx,
                         int argc,
                         char * argv[])
{
    if (argc != 0) {
        return EINVAL;
    }

    return bma2xx_write_offsets(bma2xx, 0.0, 0.0, 0.0);
}

struct stream_read_context {
    uint32_t count;
};

static bool
bma2xx_stream_read_cb(void * arg,
                      struct sensor_accel_data * sad)
{
    char buffer_x[20];
    char buffer_y[20];
    char buffer_z[20];
    struct stream_read_context * ctx;

    sensor_ftostr(sad->sad_x, buffer_x, sizeof(buffer_x));
    sensor_ftostr(sad->sad_y, buffer_y, sizeof(buffer_y));
    sensor_ftostr(sad->sad_z, buffer_z, sizeof(buffer_z));

    console_printf("x = %s y = %s z = %s\n",
                   buffer_x,
                   buffer_y,
                   buffer_z);

    ctx = (struct stream_read_context *)arg;
    ctx->count--;

    return ctx->count == 0;
}

static int
bma2xx_stream_read_cmd(struct bma2xx * bma2xx,
                       int argc,
                       char * argv[])
{
    char * end;
    struct stream_read_context ctx;

    if (argc != 1) {
        return EINVAL;
    }

    ctx.count = strtoul(argv[0], &end, 10);
    if (*end != '\0') {
        return EINVAL;
    }

    return bma2xx_stream_read(bma2xx,
                              bma2xx_stream_read_cb,
                              &ctx,
                              0);
}

static int
bma2xx_current_temp_cmd(struct bma2xx * bma2xx,
                        int argc,
                        char * argv[])
{
    float temp_c;
    int rc;
    char buffer[20];

    if (argc != 0) {
        return EINVAL;
    }

    rc = bma2xx_current_temp(bma2xx, &temp_c);
    if (rc != 0) {
        return rc;
    }

    sensor_ftostr(temp_c, buffer, sizeof(buffer));

    console_printf("temp = %s C\n", buffer);

    return 0;
}

static void
console_print_orient(const struct bma2xx_orient_xyz * orient_xyz)
{
    const char * xy_desc;
    const char * z_desc;

    switch (orient_xyz->orient_xy) {
    case BMA2XX_ORIENT_XY_PORTRAIT_UPRIGHT:
        xy_desc = "portrait-upright";
        break;
    case BMA2XX_ORIENT_XY_PORTRAIT_UPSIDE_DOWN:
        xy_desc = "portrait-upside-down";
        break;
    case BMA2XX_ORIENT_XY_LANDSCAPE_LEFT:
        xy_desc = "landscape-left";
        break;
    case BMA2XX_ORIENT_XY_LANDSCAPE_RIGHT:
        xy_desc = "landscape-right";
        break;
    default:
        xy_desc = "unknown-enum";
        break;
    }

    if (orient_xyz->downward_z) {
        z_desc = "facing-downward";
    } else {
        z_desc = "facing-upward";
    }

    console_printf("xy = %s z = %s\n", xy_desc, z_desc);
}

static int
bma2xx_current_orient_cmd(struct bma2xx * bma2xx,
                          int argc,
                          char * argv[])
{
    struct bma2xx_orient_xyz orient_xyz;
    int rc;

    if (argc != 0) {
        return EINVAL;
    }

    rc = bma2xx_current_orient(bma2xx, &orient_xyz);
    if (rc != 0) {
        return rc;
    }

    console_print_orient(&orient_xyz);

    return 0;
}

static int
bma2xx_wait_for_orient_cmd(struct bma2xx * bma2xx,
                           int argc,
                           char * argv[])
{
    struct bma2xx_orient_xyz orient_xyz;
    int rc;

    if (argc != 0) {
        return EINVAL;
    }

    rc = bma2xx_wait_for_orient(bma2xx, &orient_xyz);
    if (rc != 0) {
        return rc;
    }

    console_print_orient(&orient_xyz);
    console_printf("done!\n");

    return 0;
}

static int
bma2xx_wait_for_high_g_cmd(struct bma2xx * bma2xx,
                           int argc,
                           char * argv[])
{
    int rc;

    if (argc != 0) {
        return EINVAL;
    }

    rc = bma2xx_wait_for_high_g(bma2xx);
    if (rc != 0) {
        return rc;
    }

    console_printf("done!\n");

    return 0;
}

static int
bma2xx_wait_for_low_g_cmd(struct bma2xx * bma2xx,
                          int argc,
                          char * argv[])
{
    int rc;

    if (argc != 0) {
        return EINVAL;
    }

    rc = bma2xx_wait_for_low_g(bma2xx);
    if (rc != 0) {
        return rc;
    }

    console_printf("done!\n");

    return 0;
}

struct tap_type {
    const char * name;
    enum bma2xx_tap_type type;
};

static const struct tap_type tap_types[] = {
    {
        .name = "double",
        .type = BMA2XX_TAP_TYPE_DOUBLE,
    },
    {
        .name = "single",
        .type = BMA2XX_TAP_TYPE_SINGLE,
    },
};

static int
bma2xx_wait_for_tap_cmd(struct bma2xx * bma2xx,
                        int argc,
                        char * argv[])
{
    const struct tap_type * tap_type;
    uint8_t i;
    int rc;

    if (argc != 1) {
        return EINVAL;
    }

    tap_type = NULL;
    for (i = 0; i < sizeof(tap_types) /
                    sizeof(*tap_types); i++) {
        if (strcmp(tap_types[i].name, argv[0]) == 0) {
            tap_type = tap_types + i;
        }
    }

    if (tap_type == NULL) {
        return EINVAL;
    }

    rc = bma2xx_wait_for_tap(bma2xx, tap_type->type);
    if (rc != 0) {
        return rc;
    }

    console_printf("done!\n");

    return 0;
}

struct power_mode {
    const char * name;
    enum bma2xx_power_mode power;
};

static const struct power_mode power_modes[] = {
    {
        .name  = "normal",
        .power = BMA2XX_POWER_MODE_NORMAL,
    },
    {
        .name  = "deep-suspend",
        .power = BMA2XX_POWER_MODE_DEEP_SUSPEND,
    },
    {
        .name  = "suspend",
        .power = BMA2XX_POWER_MODE_SUSPEND,
    },
    {
        .name  = "standby",
        .power = BMA2XX_POWER_MODE_STANDBY,
    },
    {
        .name  = "lpm1",
        .power = BMA2XX_POWER_MODE_LPM_1,
    },
    {
        .name  = "lpm2",
        .power = BMA2XX_POWER_MODE_LPM_2,
    },
};

struct sleep_duration {
    const char * name;
    enum bma2xx_sleep_duration sleep;
};

static const struct sleep_duration sleep_durations[] = {
    {
        .name  = "0.5ms",
        .sleep = BMA2XX_SLEEP_DURATION_0_5_MS,
    },
    {
        .name  = "1ms",
        .sleep = BMA2XX_SLEEP_DURATION_1_MS,
    },
    {
        .name  = "2ms",
        .sleep = BMA2XX_SLEEP_DURATION_2_MS,
    },
    {
        .name  = "4ms",
        .sleep = BMA2XX_SLEEP_DURATION_4_MS,
    },
    {
        .name  = "6ms",
        .sleep = BMA2XX_SLEEP_DURATION_6_MS,
    },
    {
        .name  = "10ms",
        .sleep = BMA2XX_SLEEP_DURATION_10_MS,
    },
    {
        .name  = "25ms",
        .sleep = BMA2XX_SLEEP_DURATION_25_MS,
    },
    {
        .name  = "50ms",
        .sleep = BMA2XX_SLEEP_DURATION_50_MS,
    },
    {
        .name  = "100ms",
        .sleep = BMA2XX_SLEEP_DURATION_100_MS,
    },
    {
        .name  = "500ms",
        .sleep = BMA2XX_SLEEP_DURATION_500_MS,
    },
    {
        .name  = "1s",
        .sleep = BMA2XX_SLEEP_DURATION_1_S,
    },
};

static int
bma2xx_power_settings_cmd(struct bma2xx * bma2xx,
                          int argc,
                          char * argv[])
{
    const struct power_mode * power_mode;
    const struct sleep_duration * sleep_duration;
    uint8_t i;

    if (argc != 2) {
        return EINVAL;
    }

    power_mode = NULL;
    for (i = 0; i < sizeof(power_modes) /
                    sizeof(*power_modes); i++) {
        if (strcmp(power_modes[i].name, argv[0]) == 0) {
            power_mode = power_modes + i;
        }
    }

    sleep_duration = NULL;
    for (i = 0; i < sizeof(sleep_durations) /
                    sizeof(*sleep_durations); i++) {
        if (strcmp(sleep_durations[i].name, argv[1]) == 0) {
            sleep_duration = sleep_durations + i;
        }
    }

    if (power_mode == NULL) {
        return EINVAL;
    }
    if (sleep_duration == NULL) {
        return EINVAL;
    }

    return bma2xx_power_settings(bma2xx,
                                 power_mode->power,
                                 sleep_duration->sleep);
}

struct subcmd {
    const char * name;
    const char * help;
    int (*func)(struct bma2xx * bma2xx,
                int argc,
                char * argv[]);
};

static const struct subcmd supported_subcmds[] = {
    {
        .name = "self-test",
        .help = "<default|strict>",
        .func = bma2xx_self_test_cmd,
    },
    {
        .name = "offset-compensation",
        .help = "<x={0g|-1g|+1g}> <y={0g|-1g|+1g}> <z={0g|-1g|+1g}>",
        .func = bma2xx_offset_compensation_cmd,
    },
    {
        .name = "query-offsets",
        .help = "",
        .func = bma2xx_query_offsets_cmd,
    },
    {
        .name = "write-offsets",
        .help = "",
        .func = bma2xx_write_offsets_cmd,
    },
    {
        .name = "stream-read",
        .help = "<num-reads>",
        .func = bma2xx_stream_read_cmd,
    },
    {
        .name = "current-temp",
        .help = "",
        .func = bma2xx_current_temp_cmd,
    },
    {
        .name = "current-orient",
        .help = "",
        .func = bma2xx_current_orient_cmd,
    },
    {
        .name = "wait-for-orient",
        .help = "",
        .func = bma2xx_wait_for_orient_cmd,
    },
    {
        .name = "wait-for-high-g",
        .help = "",
        .func = bma2xx_wait_for_high_g_cmd,
    },
    {
        .name = "wait-for-low-g",
        .help = "",
        .func = bma2xx_wait_for_low_g_cmd,
    },
    {
        .name = "wait-for-tap",
        .help = "<double|single>",
        .func = bma2xx_wait_for_tap_cmd,
    },
    {
        .name = "power-settings",
        .help = "<normal|deep-suspend|suspend|standby|lpm1|lpm2>" \
                "\n                      "                        \
                "<0.5ms|1ms|2ms|4ms|6ms|10ms|25ms|50ms|100ms|500ms|1s>",
        .func = bma2xx_power_settings_cmd,
    },
};

static int
bma2xx_shell_cmd(int argc, char * argv[])
{
    struct os_dev * dev;
    struct bma2xx * bma2xx;
    const struct subcmd * subcmd;
    uint8_t i;

    dev = os_dev_open(MYNEWT_VAL(BMA2XX_SHELL_DEV_NAME), OS_TIMEOUT_NEVER, NULL);
    if (dev == NULL) {
        console_printf("failed to open bma2xx_0 device\n");
        return ENODEV;
    }

    bma2xx = (struct bma2xx *)dev;

    subcmd = NULL;
    if (argc > 1) {
        for (i = 0; i < sizeof(supported_subcmds) /
                        sizeof(*supported_subcmds); i++) {
            if (strcmp(supported_subcmds[i].name, argv[1]) == 0) {
                subcmd = supported_subcmds + i;
            }
        }

        if (subcmd == NULL) {
            console_printf("unknown %s subcommand\n", argv[1]);
        }
    }

    if (subcmd != NULL) {
        if (subcmd->func(bma2xx, argc - 2, argv + 2) != 0) {
            console_printf("could not run %s subcommand\n", argv[1]);
            console_printf("%s %s\n", subcmd->name, subcmd->help);
        }
    } else {
        for (i = 0; i < sizeof(supported_subcmds) /
                        sizeof(*supported_subcmds); i++) {
            subcmd = supported_subcmds + i;
            console_printf("%s %s\n", subcmd->name, subcmd->help);
        }
    }

    os_dev_close(dev);

    return 0;
}

static const struct shell_cmd bma2xx_shell_cmd_desc = {
    .sc_cmd      = "bma2xx",
    .sc_cmd_func = bma2xx_shell_cmd,
};

int
bma2xx_shell_init(void)
{
    return shell_cmd_register(&bma2xx_shell_cmd_desc);
}

#endif
