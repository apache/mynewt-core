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
#include <string.h>
#include <os/mynewt.h>
#include <console/console.h>
#include <shell/shell.h>
#include <parse/parse.h>
#include <hal/hal_gpio.h>
#include <hal/hal_nvreg.h>

int
nvreg_dump_func(const struct shell_cmd *cmd, int argc, char *argv[],
                struct streamer *streamer)
{
    int i;
    uint32_t val;
    int n = hal_nvreg_get_num_regs();

    for (i = 0; i < n; ++i) {
        val = hal_nvreg_read(i);
        streamer_printf(streamer, "reg[%d] = %u (0x%X)\n", i,
                        (unsigned int)val, (unsigned int)val);
    }
    return 0;
}

int
nvreg_read_func(const struct shell_cmd *cmd, int argc, char *argv[],
                struct streamer *streamer)
{
    int reg = -1;
    uint32_t val;
    int rc;

    if (argc > 1) {
        reg = parse_ll_bounds(argv[1], 0, 256, &rc);
    }
    if (reg >= 0) {
        val = hal_nvreg_read(reg);
        streamer_printf(streamer, "reg[%d] = %u (0x%X)\n", reg,
                        (unsigned int)val, (unsigned int)val);
    }
    return 0;
}

int
nvreg_write_func(const struct shell_cmd *cmd, int argc, char *argv[],
                 struct streamer *streamer)
{
    int reg = -1;
    uint32_t val = 0;
    int rc;

    if (argc > 2) {
        reg = parse_ll_bounds(argv[1], 0, 256, &rc);
        if (rc == 0) {
            val = parse_ll_bounds(argv[2], 0, 0xFFFFFFFF, &rc);
        }
    }
    if (argc >= 3 && rc == 0) {
        hal_nvreg_write(reg, val);
    } else {
        streamer_printf(streamer, "%s <reg> <val>\n", cmd->sc_cmd);
    }
    return 0;
}

int
gpio_init_out_func(const struct shell_cmd *cmd, int argc, char *argv[],
                   struct streamer *streamer)
{
    int pin = -1;
    int val = -1;
    int rc;

    if (argc > 1) {
        pin = parse_ll_bounds(argv[1], 0, 512, &rc);
    }
    if (argc > 2) {
        val = parse_ll_bounds(argv[2], 0, 1, &rc);
    } else {
        val = 0;
    }
    if (pin >= 0 && val >= 0) {
        hal_gpio_init_out(pin, val);
    }
    return 0;
}

int
gpio_write_func(const struct shell_cmd *cmd, int argc, char *argv[],
                struct streamer *streamer)
{
    int pin = -1;
    int val = -1;
    int rc;

    if (argc < 3) {
        streamer_printf(streamer, "%s <pin> 0 | 1\n", cmd->sc_cmd);
    } else {
        pin = parse_ll_bounds(argv[1], 0, 512, &rc);
        val = parse_ll_bounds(argv[2], 0, 1, &rc);
    }
    if (pin >= 0 && val >= 0) {
        hal_gpio_write(pin, val);
    }
    return 0;
}

int
gpio_toggle_func(const struct shell_cmd *cmd, int argc, char *argv[],
                 struct streamer *streamer)
{
    int pin = -1;
    int rc;

    if (argc < 2) {
        streamer_printf(streamer, "%s <pin>\n", cmd->sc_cmd);
    } else {
        pin = parse_ll_bounds(argv[1], 0, 512, &rc);
    }
    if (pin >= 0) {
        hal_gpio_toggle(pin);
    }
    return 0;
}

SHELL_MODULE_EXT_CMD(hal, nvreg_dump, nvreg_dump_func, NULL);
SHELL_MODULE_EXT_CMD(hal, nvreg_read, nvreg_read_func, NULL);
SHELL_MODULE_EXT_CMD(hal, nvreg_write, nvreg_write_func, NULL);
SHELL_MODULE_EXT_CMD(hal, gpio_init_out, gpio_init_out_func, NULL);
SHELL_MODULE_EXT_CMD(hal, gpio_write, gpio_write_func, NULL);
SHELL_MODULE_EXT_CMD(hal, gpio_toggle, gpio_toggle_func, NULL);

SHELL_MODULE_WITH_LINK_TABLE(hal);
