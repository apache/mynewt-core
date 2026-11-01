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

#include <ctype.h>
#include <string.h>
#include <os/mynewt.h>
#include <mcu/mcu.h>
#include <hal/hal_gpio.h>
#include <parse/parse.h>
#include <shell/shell.h>

/*
 * Pin can be given as a number or, on MCUs that define MCU_GPIO_PORTx()
 * macros, as port letter and pin number (PA0, A0, pb12).
 */
static int
shell_gpio_parse_pin(const char *arg)
{
    int rc;
    int pin;
#if defined(MCU_GPIO_PORTA) && defined(MCU_GPIO_PORTB)
    int port;
    const char *p = arg;

    if (toupper((unsigned char)p[0]) == 'P' && isalpha((unsigned char)p[1])) {
        p++;
    }
    if (isalpha((unsigned char)p[0]) && isdigit((unsigned char)p[1])) {
        port = toupper((unsigned char)p[0]) - 'A';
        pin = parse_ll_bounds(p + 1, 0, 31, &rc);
        if (rc) {
            return -1;
        }
        return MCU_GPIO_PORTA(pin) +
               port * (MCU_GPIO_PORTB(0) - MCU_GPIO_PORTA(0));
    }
#endif
    pin = parse_ll_bounds(arg, 0, 0xFFFF, &rc);

    return rc ? -1 : pin;
}

/* Time in ms by default, with "us" suffix in microseconds */
static int
shell_gpio_parse_time(const char *arg, uint32_t *usecs)
{
    char buf[12];
    size_t len = strlen(arg);
    uint32_t mult = 1000;
    int rc;

    if (len >= sizeof(buf)) {
        return SYS_EINVAL;
    }
    strcpy(buf, arg);
    if (len > 2 && strcmp(buf + len - 2, "us") == 0) {
        buf[len - 2] = '\0';
        mult = 1;
    } else if (len > 2 && strcmp(buf + len - 2, "ms") == 0) {
        buf[len - 2] = '\0';
    }
    *usecs = parse_ull_bounds(buf, 0, UINT32_MAX / mult, &rc) * mult;

    return rc;
}

static void
shell_gpio_delay(uint32_t usecs)
{
    os_time_t ticks;

    if (usecs >= 10000) {
        os_time_ms_to_ticks(usecs / 1000, &ticks);
        os_time_delay(ticks);
    } else {
        os_cputime_delay_usecs(usecs);
    }
}

/* Parses pin from argv[1], prints usage on error */
static int
shell_gpio_pin_arg(const struct shell_cmd *cmd, int argc, char *argv[],
                   int min_argc, const char *usage, struct streamer *streamer)
{
    int pin = -1;

    if (argc >= min_argc) {
        pin = shell_gpio_parse_pin(argv[1]);
    }
    if (pin < 0) {
        streamer_printf(streamer, "usage: %s %s\n", cmd->sc_cmd, usage);
    }

    return pin;
}

static int
shell_gpio_bit_arg(const char *arg, int *val)
{
    int rc;

    *val = parse_ll_bounds(arg, 0, 1, &rc);

    return rc;
}

static int
shell_gpio_rc(int rc, struct streamer *streamer)
{
    if (rc) {
        streamer_printf(streamer, "Error %d\n", rc);
    }

    return rc;
}

static int
shell_gpio_in(const struct shell_cmd *cmd, int argc, char *argv[],
              struct streamer *streamer)
{
    static const char usage[] = "<pin> [none|up|down]";
    hal_gpio_pull_t pull = HAL_GPIO_PULL_NONE;
    int pin;
    int rc;

    pin = shell_gpio_pin_arg(cmd, argc, argv, 2, usage, streamer);
    if (pin < 0) {
        return SYS_EINVAL;
    }
    if (argc > 2) {
        if (strcmp(argv[2], "up") == 0) {
            pull = HAL_GPIO_PULL_UP;
        } else if (strcmp(argv[2], "down") == 0) {
            pull = HAL_GPIO_PULL_DOWN;
        } else if (strcmp(argv[2], "none") != 0) {
            streamer_printf(streamer, "usage: %s %s\n", cmd->sc_cmd, usage);
            return SYS_EINVAL;
        }
    }
    rc = hal_gpio_init_in(pin, pull);
    if (rc == 0) {
        streamer_printf(streamer, "%d\n", hal_gpio_read(pin));
    }

    return shell_gpio_rc(rc, streamer);
}

static int
shell_gpio_out(const struct shell_cmd *cmd, int argc, char *argv[],
               struct streamer *streamer)
{
    static const char usage[] = "<pin> [0|1]";
    int pin;
    int val = 0;

    pin = shell_gpio_pin_arg(cmd, argc, argv, 2, usage, streamer);
    if (pin < 0 || (argc > 2 && shell_gpio_bit_arg(argv[2], &val))) {
        return SYS_EINVAL;
    }

    return shell_gpio_rc(hal_gpio_init_out(pin, val), streamer);
}

static int
shell_gpio_read(const struct shell_cmd *cmd, int argc, char *argv[],
                struct streamer *streamer)
{
    int pin;

    pin = shell_gpio_pin_arg(cmd, argc, argv, 2, "<pin>", streamer);
    if (pin < 0) {
        return SYS_EINVAL;
    }
    streamer_printf(streamer, "%d\n", hal_gpio_read(pin));

    return 0;
}

static int
shell_gpio_write(const struct shell_cmd *cmd, int argc, char *argv[],
                 struct streamer *streamer)
{
    int pin;
    int val;

    pin = shell_gpio_pin_arg(cmd, argc, argv, 3, "<pin> 0|1", streamer);
    if (pin < 0 || shell_gpio_bit_arg(argv[2], &val)) {
        return SYS_EINVAL;
    }
    hal_gpio_write(pin, val);

    return 0;
}

static int
shell_gpio_toggle(const struct shell_cmd *cmd, int argc, char *argv[],
                  struct streamer *streamer)
{
    int pin;

    pin = shell_gpio_pin_arg(cmd, argc, argv, 2, "<pin>", streamer);
    if (pin < 0) {
        return SYS_EINVAL;
    }
    streamer_printf(streamer, "%d\n", hal_gpio_toggle(pin));

    return 0;
}

static int
shell_gpio_pulse(const struct shell_cmd *cmd, int argc, char *argv[],
                 struct streamer *streamer)
{
    static const char usage[] = "<pin> <time>[ms|us] [0|1]";
    uint32_t usecs;
    int pin;
    int val;

    pin = shell_gpio_pin_arg(cmd, argc, argv, 3, usage, streamer);
    if (pin < 0) {
        return SYS_EINVAL;
    }
    if (shell_gpio_parse_time(argv[2], &usecs)) {
        streamer_printf(streamer, "usage: %s %s\n", cmd->sc_cmd, usage);
        return SYS_EINVAL;
    }
    /* Pulse level defaults to opposite of current pin state */
    if (argc > 3) {
        if (shell_gpio_bit_arg(argv[3], &val)) {
            return SYS_EINVAL;
        }
    } else {
        val = !hal_gpio_read(pin);
    }
    hal_gpio_write(pin, val);
    shell_gpio_delay(usecs);
    hal_gpio_write(pin, !val);

    return 0;
}

static int
shell_gpio_deinit(const struct shell_cmd *cmd, int argc, char *argv[],
                  struct streamer *streamer)
{
    int pin;

    pin = shell_gpio_pin_arg(cmd, argc, argv, 2, "<pin>", streamer);
    if (pin < 0) {
        return SYS_EINVAL;
    }

    return shell_gpio_rc(hal_gpio_deinit(pin), streamer);
}

#if MYNEWT_VAL(SHELL_CMD_HELP)
#define SHELL_GPIO_HELP(name_, summary_, usage_)                        \
    static const struct shell_cmd_help shell_gpio_##name_##_help = {    \
        .summary = summary_,                                            \
        .usage = usage_,                                                \
    }
#else
#define SHELL_GPIO_HELP(name_, summary_, usage_)
#endif

SHELL_GPIO_HELP(in, "configure pin as input, print value",
                "<pin> [none|up|down]");
SHELL_GPIO_HELP(out, "configure pin as output", "<pin> [0|1]");
SHELL_GPIO_HELP(read, "print pin value", "<pin>");
SHELL_GPIO_HELP(write, "set output value", "<pin> 0|1");
SHELL_GPIO_HELP(toggle, "toggle output, print new value", "<pin>");
SHELL_GPIO_HELP(pulse, "set level (default opposite of current) for time, "
                "then restore", "<pin> <time>[ms|us] [0|1]");
SHELL_GPIO_HELP(deinit, "release pin", "<pin>");

static const struct shell_cmd shell_gpio_commands[] = {
    SHELL_CMD_EXT("in", shell_gpio_in, &shell_gpio_in_help),
    SHELL_CMD_EXT("out", shell_gpio_out, &shell_gpio_out_help),
    SHELL_CMD_EXT("read", shell_gpio_read, &shell_gpio_read_help),
    SHELL_CMD_EXT("write", shell_gpio_write, &shell_gpio_write_help),
    SHELL_CMD_EXT("toggle", shell_gpio_toggle, &shell_gpio_toggle_help),
    SHELL_CMD_EXT("pulse", shell_gpio_pulse, &shell_gpio_pulse_help),
    SHELL_CMD_EXT("deinit", shell_gpio_deinit, &shell_gpio_deinit_help),
};

SHELL_MODULE_WITH_TABLE(gpio, shell_gpio_commands);
