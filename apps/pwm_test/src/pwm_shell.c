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
#include "os/os_dev.h"
#include "console/console.h"
#include "shell/shell.h"
#include "pwm/pwm.h"
#include "bsp/bsp.h"

#include "pwm_shell.h"

#if MYNEWT_VAL(SOFT_PWM)
#define MAX_PWM_DEVICES (4 + MYNEWT_VAL(SOFT_PWM_DEVS))
#else
#define MAX_PWM_DEVICES (4)
#endif
static struct pwm_dev * pwm_devs[MAX_PWM_DEVICES];

extern void pwm_init(struct pwm_dev *pwm_dev, int pin);

static int
store_pwm(char *pwm_name, struct pwm_dev *dev)
{
    int i;

    if (pwm_name[0] == 's') {
        /* in case of spwmx*/
        i = atoi(pwm_name + 4);
        /* starting from 4 we have soft pwms in the table */
        i += 4;
    } else {
        i = atoi(pwm_name + 3);
    }

    if (i > MAX_PWM_DEVICES || pwm_devs[i] != NULL) {
        return -1;
    }

    pwm_devs[i] = dev;

    return 0;
}

static int
get_stored_pwm_idx(char *pwm_name)
{
    int i;

    if (pwm_name[0] == 's') {
        /* in case of spwmx*/
        i = atoi(pwm_name + 4);
        /* starting from 4 we have soft pwms in the table */
        i += 4;
    } else {
        i = atoi(pwm_name + 3);
    }

    if (i > MAX_PWM_DEVICES) {
        console_printf("Too high pwm index. Increase MAX_PWM_DEVICES\n");
        return -2;
    }

    return pwm_devs[i] ? i : -1;
}

#if MYNEWT_VAL(SHELL_CMD_HELP)
static const struct shell_param test_suite_params[] = {
    {"dev", "pwm device to stop, usage: =[pwm0|pwm1|pwmn], default: pwm0"},
    {"pin", "pin number, default: not set (-1)"},
    {NULL, NULL}
};

static const struct shell_cmd_help test_suite_help = {
    .summary = "Test suite. Note: requires cycle and seq_end support",
    .usage = NULL,
    .params = test_suite_params,
};
#endif

static int
cmd_test_suite(int argc, char **argv)
{
    char *pwm = PWM_TEST_DEV;
    char *tmp;
    int pin = -1;
    int i;
    struct pwm_dev *pwm_dev;

    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "dev=", 4) == 0) {
            pwm = strtok(argv[i], "=");
            pwm = strtok(NULL, "=");
        } else if (strncmp(argv[i], "pin=", 4) == 0) {
            tmp = strtok(argv[i], "=");
            tmp = strtok(NULL, "=");
            pin = atoi(tmp);
        } else {
            console_printf("Unknown parameter %s, use help\n", argv[i]);
            return 0;
        }
    }

    if (get_stored_pwm_idx(pwm) != -1) {
        console_printf("%s is already busy\n", pwm);
        return 0;
    }

    pwm_dev = (struct pwm_dev *)os_dev_open(pwm, 0, NULL);
    assert(pwm_dev);

    store_pwm(pwm, pwm_dev);

    pwm_init(pwm_dev, pin);

    return 0;
}

#if MYNEWT_VAL(SHELL_CMD_HELP)
static const struct shell_cmd_help list_help = {
    .summary = "Print list of pwm devices",
    .usage = NULL,
    .params = NULL,
};
#endif

static int
pwm_dev_ls(struct os_dev *dev, void *arg)
{
    if (strncmp("pwm", dev->od_name, 3) != 0) {
        return 0;
    }

    console_printf("%4d %3x %s\n",
                   dev->od_open_ref, dev->od_flags, dev->od_name);
    return 0;
}

static int
cmd_list(int argc, char **argv)
{
    console_printf("%4s %3s %s\n", "ref", "flg", "name");
    os_dev_walk(pwm_dev_ls, NULL);

    return 0;
}

#if MYNEWT_VAL(SHELL_CMD_HELP)
static const struct shell_param start_params[] = {
    {"dev", "pwm device to open, usage: =[pwm0|pwm1|pwmn], default: pwm0"},
    {"freq", "frequency to set in Hz, default: 200 Hz"},
    {"dc", "duty cycle, usage: =[0-100], default: 50"},
    {"pin", "pin number, default: LED_BLINK_PIN for the bsp"},
    {"chan", "channel number, default: 0"},
    {NULL, NULL}
};

static const struct shell_cmd_help start_help = {
    .summary = "Open and enable pwm device",
    .usage = NULL,
    .params = start_params,
};
#endif

static int
cmd_start(int argc, char **argv)
{
    uint8_t pin = LED_BLINK_PIN;
    char *tmp;
    char *pwm = "pwm0";
    int freq = 200;
    int dc = 50;
    int chan = 0;
    int i;
    int rc;
    int top;
    struct pwm_dev *dev;
    struct pwm_chan_cfg chan_conf = { 0 };

    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "dev=", 4) == 0) {
            pwm = strtok(argv[i], "=");
            pwm = strtok(NULL, "=");
        } else if (strncmp(argv[i], "freq=", 5) == 0) {
            tmp = strtok(argv[i], "=");
            tmp = strtok(NULL, "=");
            freq = atoi(tmp);
        } else if (strncmp(argv[i], "dc=", 3) == 0) {
            tmp = strtok(argv[i], "=");
            tmp = strtok(NULL, "=");
            dc = atoi(tmp);
            if (dc > 100 || dc < 0) {
                console_printf("Incorrect duty cycle. See help.\n");
                return 0;
            }
        } else if (strncmp(argv[i], "pin=", 4) == 0) {
            tmp = strtok(argv[i], "=");
            tmp = strtok(NULL, "=");
            pin = atoi(tmp);
        } else if (strncmp(argv[i], "chan=", 5) == 0) {
            tmp = strtok(argv[i], "=");
            tmp = strtok(NULL, "=");
            chan = atoi(tmp);
        } else {
            console_printf("Unknown parameter %s, use help\n", argv[i]);
            return 0;
        }
    }

    console_printf("Opening %s, pin=%d, freq=%d, dc=%d, chan=%d\n", pwm, pin, freq, dc, chan);

    if (get_stored_pwm_idx(pwm) != -1) {
        console_printf("%s is already busy\n", pwm);
        return 0;
    }

    dev = (struct pwm_dev *)os_dev_open(pwm, 0, NULL);
    if (!dev) {
        console_printf("Could not open %s\n", pwm);
        return 0;
    }

    chan_conf.pin = pin;
    rc = pwm_configure_channel(dev, chan, &chan_conf);
    if (rc) {
        console_printf("Could configure channel %d on %s\n", chan, pwm);
        return 0;
    }

   rc = pwm_set_frequency(dev, freq);
   if (rc < 0) {
       console_printf("Could configure frequency %s\n", pwm);
       return 0;
   }

   console_printf("Set freq=%d to %s\n", rc, pwm);

   top = pwm_get_top_value(dev);

   rc = pwm_set_duty_cycle(dev, chan, ((top * dc) / 100));
   if (rc) {
      console_printf("Could configure duty cycle %d on %s\n", ((top * dc) / 100), pwm);
      return 0;
   }

   store_pwm(pwm, dev);

   return pwm_enable(dev);
}

#if MYNEWT_VAL(SHELL_CMD_HELP)
static const struct shell_param stop_params[] = {
    {"dev", "pwm device to stop, usage: =[pwm0|pwm1|pwmn], default: pwm0"},
    {NULL, NULL}
};

static const struct shell_cmd_help stop_help = {
    .summary = "Disable and close pwm device",
    .usage = NULL,
    .params = stop_params,
};
#endif

static int
cmd_stop(int argc, char **argv)
{
    char *pwm = "pwm0";
    int i;
    int rc;

    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "dev=", 4) == 0) {
            pwm = strtok(argv[i], "=");
            pwm = strtok(NULL, "=");
        } else {
            console_printf("Unknown parameter %s, use help\n", argv[i]);
            return 0;
        }
    }

    i = get_stored_pwm_idx(pwm);
    if (i < 0) {
        console_printf("Could not find stored %s\n", pwm);
        return 0;
    }

    rc = pwm_disable(pwm_devs[i]);
    if (rc) {
        console_printf("Could not disable %s, err=%d\n", pwm, rc);
        return 0;
    }

    rc = os_dev_close((struct os_dev *)pwm_devs[i]);
    if (rc) {
        console_printf("Could not close os_dev %s, err=%d\n", pwm, rc);
        return 0;
    }

    pwm_devs[i] = NULL;

    return 0;
}

#if MYNEWT_VAL(SHELL_CMD_HELP)
static const struct shell_param reconf_params[] = {
    {"dev", "pwm device, usage: =[pwm0|pwm1|pwmn], default: pwm0"},
    {"freq", "frequency to set in Hz, default: current Hz"},
    {"dc", "duty cycle, usage: =[0-100], default: 50"},
    {"chan", "channel number, default: 0"},
    {NULL, NULL}
};

static const struct shell_cmd_help reconf_help = {
    .summary = "Reconfigure pwm device",
    .usage = NULL,
    .params = reconf_params,
};
#endif

static int
cmd_reconf(int argc, char **argv)
{
    char *pwm = "pwm0";
    char *tmp;
    int i;
    int rc;
    int freq = 0;
    int top;
    int dc = 50;
    int chan = 0;

    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "dev=", 4) == 0) {
            pwm = strtok(argv[i], "=");
            pwm = strtok(NULL, "=");
        } else if (strncmp(argv[i], "freq=", 5) == 0) {
            tmp = strtok(argv[i], "=");
            tmp = strtok(NULL, "=");
            freq = atoi(tmp);
        }  else if (strncmp(argv[i], "dc=", 3) == 0) {
            tmp = strtok(argv[i], "=");
            tmp = strtok(NULL, "=");
            dc = atoi(tmp);
            if (dc > 100 || dc < 0) {
                console_printf("Incorrect duty cycle. See help.\n");
                return 0;
            }
        } else if (strncmp(argv[i], "chan=", 5) == 0) {
            tmp = strtok(argv[i], "=");
            tmp = strtok(NULL, "=");
            chan = atoi(tmp);
        } else {
            console_printf("Unknown parameter %s, use help\n", argv[i]);
            return 0;
        }
    }

    i = get_stored_pwm_idx(pwm);
    if (i < 0) {
        console_printf("Could not find stored %s\n", pwm);
        return 0;
    }

    rc = pwm_disable(pwm_devs[i]);
    if (rc < 0) {
        console_printf("Could not disable %s, err=%d\n", pwm, rc);
        return 0;
    }

    if (freq == 0) {
        freq = pwm_get_clock_freq(pwm_devs[i]);
    }

    rc = pwm_set_frequency(pwm_devs[i], freq);
    if (rc < 0) {
        console_printf("Could not set os_dev %s, err=%d\n", pwm, rc);
        return 0;
    }

    console_printf("Set freq=%d to %s\n", rc, pwm);

    top = pwm_get_top_value(pwm_devs[i]);

    rc = pwm_set_duty_cycle(pwm_devs[i], chan, ((top * dc) / 100));
    if (rc) {
       console_printf("Could configure duty cycle %d on %s err=%d\n", ((top * dc) / 100), pwm, rc);
       return 0;
    }

    pwm_enable(pwm_devs[i]);

    return 0;
}

static const struct shell_cmd pwm_shell_commands[] = {
    {
        .sc_cmd = "list",
        .sc_cmd_func = cmd_list,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &list_help,
#endif
    },
    {
        .sc_cmd = "start",
        .sc_cmd_func = cmd_start,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &start_help,
#endif
    },
    {
        .sc_cmd = "stop",
        .sc_cmd_func = cmd_stop,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &stop_help,
#endif
    },
    {
        .sc_cmd = "reconf",
        .sc_cmd_func = cmd_reconf,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &reconf_help,
#endif
    },
    {
        .sc_cmd = "test_suite",
        .sc_cmd_func = cmd_test_suite,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &test_suite_help,
#endif
    },
    {}
};

void
pwm_shell_init(void)
{
    shell_register("pwm_shell", pwm_shell_commands);
    shell_register_default_module("pwm_shell");
}
