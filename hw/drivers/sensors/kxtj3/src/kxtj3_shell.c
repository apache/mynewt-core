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

#include "os/mynewt.h"
#include "syscfg/syscfg.h"
#include "kxtj3/kxtj3.h"
#include "kxtj3_priv.h"
#include "console/console.h"
#include "shell/shell.h"
#include "parse/parse.h"

#if MYNEWT_VAL(KXTJ3_CLI)

static struct sensor_itf g_sensor_itf = {
    .si_type = MYNEWT_VAL(KXTJ3_SHELL_ITF_TYPE),
    .si_num = MYNEWT_VAL(KXTJ3_SHELL_ITF_NUM),
    .si_addr = MYNEWT_VAL(KXTJ3_SHELL_ITF_ADDR),
};

struct key_value {
    const char *key;
    uint8_t value;
};

static const struct key_value odr_map[] = {
    {
        .key  = "1600hz",
        .value = KXTJ3_ODR_1600HZ,
    },
    {
        .key  = "800hz",
        .value = KXTJ3_ODR_800HZ,
    },
    {
        .key  = "400hz",
        .value = KXTJ3_ODR_400HZ,
    },
    {
        .key  = "200hz",
        .value = KXTJ3_ODR_200HZ,
    },
    {
        .key  = "100hz",
        .value = KXTJ3_ODR_100HZ,
    },
    {
        .key  = "50hz",
        .value = KXTJ3_ODR_50HZ,
    },
    {
        .key  = "25hz",
        .value = KXTJ3_ODR_25HZ,
    },
    {
        .key  = "12.5hz",
        .value = KXTJ3_ODR_12P5HZ,
    },
    {
        .key  = "6.25hz",
        .value = KXTJ3_ODR_6P25HZ,
    },
    {
        .key  = "3.125hz",
        .value = KXTJ3_ODR_3P125HZ,
    },
    {
        .key  = "1.563hz",
        .value = KXTJ3_ODR_1P563HZ,
    },
    {
        .key  = "0.781hz",
        .value = KXTJ3_ODR_0P781HZ,
    },
};

struct key_value *kxtj3_get_odr(char *key)
{
    struct key_value *odr = NULL;
    int i;

    for (i = 0; i < sizeof(odr_map) /
                    sizeof(*odr_map); i++) {
        if (strcmp(odr_map[i].key, key) == 0) {
            odr = (struct key_value *)&odr_map[i];
            break;
        }
    }

    return odr;
}

static const struct key_value perf_mode_map[] = {
    {
        .key  = "8bit",
        .value = KXTJ3_PERF_MODE_LOW_POWER_8BIT,
    },
    {
        .key  = "12bit",
        .value = KXTJ3_PERF_MODE_HIGH_RES_12BIT,
    },
    {
        .key  = "14bit",
        .value = KXTJ3_PERF_MODE_HIGH_RES_14BIT,
    },
};

struct key_value *kxtj3_get_perf_mode(char *key)
{
    struct key_value *perf_mode = NULL;
    int i;

    for (i = 0; i < sizeof(perf_mode_map) /
                    sizeof(*perf_mode_map); i++) {
        if (strcmp(perf_mode_map[i].key, key) == 0) {
            perf_mode = (struct key_value *)&perf_mode_map[i];
            break;
        }
    }

    return perf_mode;
}

static const struct key_value grange_map[] = {
    {
        .key  = "2g",
        .value = KXTJ3_GRANGE_2G,
    },
    {
        .key  = "4g",
        .value = KXTJ3_GRANGE_4G,
    },
    {
        .key  = "8g",
        .value = KXTJ3_GRANGE_8G,
    },
    {
        .key  = "16g",
        .value = KXTJ3_GRANGE_16G,
    },
};

struct key_value *kxtj3_get_grange(char *key)
{
    struct key_value *grange = NULL;
    int i;

    for (i = 0; i < sizeof(grange_map) /
                    sizeof(*grange_map); i++) {
        if (strcmp(grange_map[i].key, key) == 0) {
            grange = (struct key_value *)&grange_map[i];
            break;
        }
    }

    return grange;
}

static int
kxtj3_cfg_test(struct kxtj3 *kxtj3,
               int argc,
               char *argv[])
{
    struct kxtj3_cfg cfg;
    int rc;
    struct key_value *perf_mode;
    struct key_value *grange;
    struct key_value *odr;

    if(argc < 2) {
        console_printf("kxtj3 argc < 2\n");
        return 0;
    }

    /* fixed cfg parameter order */
    perf_mode = kxtj3_get_perf_mode(argv[0]);
    if (perf_mode == NULL) {
        console_printf("kxtj3 perf_mode not found\n");
        return 0;
    }

    grange = kxtj3_get_grange(argv[1]);
    if (grange == NULL) {
        console_printf("kxtj3 grange not found\n");
        return 0;
    }

    odr = kxtj3_get_odr(argv[2]);
    if (odr == NULL) {
        console_printf("kxtj3 odr not found\n");
        return 0;
    }

    /* get current config */
    cfg = kxtj3->cfg;

    /* overwrite accel config */
    cfg.oper_mode = KXTJ3_OPER_MODE_OPERATING;
    cfg.perf_mode = perf_mode->value;
    cfg.grange = grange->value;
    cfg.odr = odr->value;
    cfg.sensors_mask = SENSOR_TYPE_ACCELEROMETER;

    rc = kxtj3_config(kxtj3, &cfg);
    assert(rc == 0);

    console_printf("kxtj3_config: set ok\n");
    return 0;
}

static const struct key_value wuf_odr_map[] = {
    {
        .key  = "100hz",
        .value = KXTJ3_WUF_ODR_100HZ,
    },
    {
        .key  = "50hz",
        .value = KXTJ3_WUF_ODR_50HZ,
    },
    {
        .key  = "25hz",
        .value = KXTJ3_WUF_ODR_25HZ,
    },
    {
        .key  = "12.5hz",
        .value = KXTJ3_WUF_ODR_12P5HZ,
    },
    {
        .key  = "6.25hz",
        .value = KXTJ3_WUF_ODR_6P25HZ,
    },
    {
        .key  = "3.125hz",
        .value = KXTJ3_WUF_ODR_3P125HZ,
    },
    {
        .key  = "1.563hz",
        .value = KXTJ3_WUF_ODR_1P563HZ,
    },
    {
        .key  = "0.781hz",
        .value = KXTJ3_WUF_ODR_0P781HZ,
    },
};

struct key_value *kxtj3_get_wuf_odr(char *key)
{
    struct key_value *wuf_odr = NULL;
    int i;

    for (i = 0; i < sizeof(wuf_odr_map) /
                    sizeof(*wuf_odr_map); i++) {
        if (strcmp(wuf_odr_map[i].key, key) == 0) {
            wuf_odr = (struct key_value *)&wuf_odr_map[i];
            break;
        }
    }

    return wuf_odr;
}

static int
kxtj3_wuf_cfg_test(struct kxtj3 *kxtj3,
               int argc,
               char *argv[])
{
    struct kxtj3_cfg cfg;
    int rc;
    struct key_value *wuf_odr;
    float delay;
    float threshold;
    uint16_t value;

    if(argc < 2) {
        console_printf("kxtj3_wuf_cfg argc < 2\n");
        return 0;
    }

    /* fixed wuf cfg parameter order */
    wuf_odr = kxtj3_get_wuf_odr(argv[0]);
    if (wuf_odr == NULL) {
        console_printf("kxtj3 wuf_odr not found\n");
        return 0;
    }

    value = parse_ll_bounds(argv[1], 1, UINT16_MAX, &rc);
    if (rc != 0) {
        console_printf("kxtj3 incorrect threshold\n");
        return 0;
    }
    threshold = value / 1000.0F; /* milli-m/s2 to m/s2 */

    value = parse_ll_bounds(argv[2], 1, UINT16_MAX, &rc);
    if (rc != 0) {
        console_printf("kxtj3 incorrect delay\n");
        return 0;
    }
    delay = value / 1000.0F; /* milli-sec to sec */

    /* get current config */
    cfg = kxtj3->cfg;

    /* overwrite wake-up config */
    cfg.oper_mode = KXTJ3_OPER_MODE_OPERATING;
    cfg.wuf.odr = wuf_odr->value;
    cfg.wuf.delay = delay;
    cfg.wuf.threshold = threshold;
    cfg.sensors_mask = SENSOR_TYPE_ACCELEROMETER;

    rc = kxtj3_config(kxtj3, &cfg);
    assert(rc == 0);

    return 0;
}

static int
kxtj3_wuf_wait_test(struct kxtj3 *kxtj3,
               int argc,
               char *argv[])
{
    int rc;

    console_printf("kxtj3_wuf_wait_test: wait_for_wakeup start \n");
    rc = kxtj3_wait_for_wakeup(&kxtj3->sensor);
    assert(rc == 0);
    console_printf("kxtj3_wuf_wait_test: wait_for_wakeup done\n");

    return 0;
}

static const uint8_t dump_regs[] = {
    KXTJ3_INT_SOURCE1,
    KXTJ3_INT_SOURCE2,
    KXTJ3_STATUS_REG,
    //KXTJ3_INT_REL, /* don't read INT_REL */
    KXTJ3_CTRL_REG1,
    KXTJ3_CTRL_REG2,
    KXTJ3_INT_CTRL_REG1,
    KXTJ3_INT_CTRL_REG2,
    KXTJ3_DATA_CTRL_REG,
    KXTJ3_WAKEUP_COUNTER,
    KXTJ3_NA_COUNTER,
    KXTJ3_SELF_TEST,
    KXTJ3_WAKEUP_THRESHOLD_H,
    KXTJ3_WAKEUP_THRESHOLD_L
};

static int
kxtj3_dump(struct kxtj3 *kxtj3,
           int argc,
           char *argv[])
{
    int rc;
    int i;
    uint8_t reg_val;

    console_printf("kxtj3 [reg, val] - dump start \n");
    for (i = 0; i < sizeof(dump_regs) /
                    sizeof(*dump_regs); i++) {
        rc = kxtj3_read8(&g_sensor_itf, dump_regs[i], &reg_val);
        if (rc) {
            goto err;
        }
        console_printf("0x%x 0x%x\n",dump_regs[i], reg_val);
    }
    console_printf("kxtj3 [reg, val] - dump end \n");
    return 0;
err:
    return rc;
}

struct subcmd {
    const char *name;
    const char *help;
    int (*func)(struct kxtj3 *kxtj3,
                int argc,
                char *argv[]);
};

static const struct subcmd supported_subcmds[] = {
    {
        .name = "cfg",
        .help = "set kxtj3 config, [bits] [grange] [odr]\n"
                "example: kxtj3 cfg 12bit 4g 50hz\n",
        .func = kxtj3_cfg_test,
    },
    {
        .name = "wuf_cfg",
        .help = "set kxtj3 wuf config, [odr] [threshold(milli-m/s2)] [delay(milli-sec)]\n"
                "example: kxtj3 wuf_cfg 25hz 3600 250\n",
        .func = kxtj3_wuf_cfg_test,
    },
    {
        .name = "wuf_wait",
        .help = "wait for wuf interrupt\n"
                "example: kxtj3 wuf_wait\n",
        .func = kxtj3_wuf_wait_test,
    },
    {
        .name = "dump",
        .help = "dump kxtj3 registers\n",
        .func = kxtj3_dump,
    },
};

static int
kxtj3_shell_cmd(int argc, char *argv[])
{
    struct os_dev *dev;
    struct kxtj3 *kxtj3;
    const struct subcmd *subcmd;
    uint8_t i;

    dev = os_dev_open(MYNEWT_VAL(KXTJ3_SHELL_DEV_NAME), OS_TIMEOUT_NEVER, NULL);
    if (dev == NULL) {
        console_printf("failed to open kxtj3_0 device\n");
        return ENODEV;
    }

    kxtj3 = (struct kxtj3 *)dev;

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
        if (subcmd->func(kxtj3, argc - 2, argv + 2) != 0) {
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

static const struct shell_cmd kxtj3_shell_cmd_desc = {
    .sc_cmd      = "kxtj3",
    .sc_cmd_func = kxtj3_shell_cmd,
};

int
kxtj3_shell_init(void)
{
    return shell_cmd_register(&kxtj3_shell_cmd_desc);
}

#endif

