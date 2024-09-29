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

#include <os/mynewt.h>

#include <console/console.h>
#include <shell/shell.h>
#include <parse/parse.h>
#include <stdio.h>
#include <mcu/mcu.h>
#include <cs47l63/cs47l63_driver.h>

#define HELP(a) NULL

static struct cs47l63_dev *cs47l63;

static char cs47l63_name[20] = MYNEWT_VAL(CS47L63_SHELL_DEV_NAME);

static int
open_dev(void)
{
    if (cs47l63 == NULL) {
        cs47l63 = (struct cs47l63_dev *)os_dev_open(cs47l63_name, 0, NULL);
        if (cs47l63 == NULL) {
            console_printf("Can't access %s device\n", cs47l63_name);
        }
    }
    return cs47l63 != NULL;
}

static int
cs47l63_shell_cmd_read(int argc, char **argv)
{
    int status;
    uint32_t reg;
    uint32_t val;

    if (argc < 2) {
        console_printf("Register address required\n");
        return 0;
    }
    reg = (uint32_t)parse_ull_bounds(argv[1], 0, 0xFFFFFFFF, &status);
    if (status) {
        console_printf("Invalid register address %s\n", argv[1]);
        return 0;
    }

    if (open_dev()) {
        cs47l63_reg_read(cs47l63, reg, &val);
        console_printf("0x%X\n", (int)val);
    }

    return 0;
}

static int
cs47l63_shell_cmd_write(int argc, char **argv)
{
    int status;
    uint32_t reg;
    uint32_t val;

    if (argc < 3) {
        console_printf("Register address and value required\n");
        return 0;
    }
    reg = (uint32_t)parse_ull_bounds(argv[1], 0, 0xFFFFFFFF, &status);
    if (status) {
        console_printf("Invalid register address %s\n", argv[1]);
        return 0;
    }
    val = (uint32_t)parse_ull_bounds(argv[2], 0, 0xFFFFFFFF, &status);
    if (status) {
        console_printf("Invalid register value %s\n", argv[2]);
        return 0;
    }
    if (open_dev()) {
        cs47l63_reg_write(cs47l63, reg, val);
    }

    return 0;
}

static int
cs47l63_shell_cmd_tone(int argc, char **argv)
{
    int status;
    uint32_t tone = 1;

    if (argc > 1) {
        tone = (uint32_t)parse_ull_bounds(argv[1], 0, 1, &status);
        if (status) {
            console_printf("1 enables 1kHz tone, 0 disables\n");
            tone = 1;
        }
    }
    if (open_dev()) {
        cs47l63_reg_write(cs47l63, CS47L63_TONE_GENERATOR1, tone);
        if (tone) {
            cs47l63_reg_write(cs47l63, CS47L63_OUT1L_INPUT3, 0x808004);
        } else {
            cs47l63_reg_write(cs47l63, CS47L63_OUT1L_INPUT3, 0x800000);
        }
    }

    return 0;
}

static int
cs47l63_shell_cmd_vol(int argc, char **argv)
{
    int status;
    int8_t vol = 0;

    if (argc > 1) {
        vol = (uint8_t)parse_ll_bounds(argv[1], -64, 31, &status);
        if (status) {
            console_printf("Volume should be in range -64..31 dB\n");
            return 0;
        }
        if (open_dev()) {
            cs47l63_volume_set(cs47l63, (int8_t)vol);
        }
    } else {
        if (open_dev()) {
            cs47l63_volume_get(cs47l63, &vol);
            console_printf("Current volume %d dB", vol);
        }
    }

    return 0;
}

static int
cs47l63_shell_cmd_clk_on_gpio9(int argc, char **argv)
{
    int status;
    uint32_t on = 1;

    if (argc > 1) {
        on = (uint32_t)parse_ull_bounds(argv[1], 0, 1, &status);
        if (status) {
            console_printf("1 fll clock / 10 to be routed to GPIO9\n");
            on = 1;
        }
    }
    if (open_dev()) {
        cs47l63_reg_write(cs47l63, CS47L63_FLL1_GPIO_CLOCK, 10 << 1 | on);
        if (on) {
            cs47l63_reg_write(cs47l63, CS47L63_GPIO9_CTRL1, 0x8010);
        } else {
            cs47l63_reg_write(cs47l63, CS47L63_GPIO9_CTRL1, 0xE1000001);
        }
    }

    return 0;
}

static const struct shell_cmd cs47l63_cmds[] = {
    SHELL_CMD("vol", cs47l63_shell_cmd_vol, HELP(enable)),
    SHELL_CMD("r", cs47l63_shell_cmd_read, HELP(enable)),
    SHELL_CMD("w", cs47l63_shell_cmd_write, HELP(disable)),
    SHELL_CMD("tone", cs47l63_shell_cmd_tone, HELP(disable)),
    SHELL_CMD("clk_on_gpio9", cs47l63_shell_cmd_clk_on_gpio9, HELP(disable)),
    SHELL_CMD(NULL, NULL, NULL)
};

#if MYNEWT_VAL(SHELL_COMPAT)
static const struct shell_cmd cs47l63_shell_cmd_struct;

static int
cs47l63_help(void)
{
    console_printf("%s cmd\n", cs47l63_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\thelp\n");
    console_printf("\tvol <val>\n");
    console_printf("\tr <reg_addr>\n");
    console_printf("\tw <reg_addr> <value>\n");
    console_printf("\ttone [0|1]\n");
    console_printf("\tclk_on_gpio9 [0|1]\n");
    return 0;
}

static int
cs47l63_shell_cmd(int argc, char **argv)
{
    const struct shell_cmd *cmd = cs47l63_cmds;

    argv++;
    argc--;
    if (argc == 0 || strcmp(argv[0], "help") == 0) {
        return cs47l63_help();
    }

    for (; cmd->sc_cmd; ++cmd) {
        if (strcmp(cmd->sc_cmd, argv[0]) == 0) {
            return cmd->sc_cmd_func(argc, argv);
        }
    }

    return -1;
}

static const struct shell_cmd cs47l63_shell_cmd_struct = {
    .sc_cmd = "cs47l63",
    .sc_cmd_func = cs47l63_shell_cmd
};
#endif

int
cs47l63_shell_init(struct cs47l63_dev *dev)
{
    int rc;
    (void)dev;

#if MYNEWT_VAL(SHELL_COMPAT)
    rc = shell_cmd_register(&cs47l63_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

    rc = shell_register("cs47l63", cs47l63_cmds);

    return rc;
}
