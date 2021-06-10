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

#include <stdio.h>
#include <ctype.h>

#include <os/mynewt.h>
#include <streamer/streamer.h>
#include <console/console.h>
#include <shell/shell.h>
#include <parse/parse.h>
#include <max3107/max3107.h>

static struct max3107_dev *max3107_dev;
static struct streamer *shell_stream;

#if MYNEWT_VAL(SHELL_CMD_HELP)
#define HELP(a) &(max3107_##a##_help)
static const struct shell_param max3107_open_param_help = {
    .param_name = "<device>",
    .help = "device to open",
};
static const struct shell_cmd_help max3107_open_help = {
    .summary = "Opens device",
    .usage = "open [name]",
    .params = &max3107_open_param_help,
};

static const struct shell_cmd_help max3107_close_help = {
    .summary = "Closes device",
    .usage = NULL,
    .params = NULL,
};

static const struct shell_cmd_help max3107_write_help = {
    .summary = "Write test data to UART",
    .usage = "write some text",
    .params = NULL,
};

static const struct shell_cmd_help max3107_reg_help = {
    .summary = "Read or write to register",
    .usage = "reg <reg> [<val>]",
    .params = NULL,
};

static const struct shell_cmd_help max3107_dump_help = {
    .summary = "Read all registers",
    .usage = "dump",
    .params = NULL,
};

#else
#define HELP(a) NULL
#endif

static bool dev_writable;
static struct streamer *shell_stream;

static void
max3107_writable(struct max3107_dev *dev)
{
    dev_writable = true;
}

static void
max3107_readable(struct max3107_dev *dev)
{
    uint8_t buf[100];
    int rc;
    int i;
    int graph = 0;
    int spaces = 0;

    rc = max3107_read(dev, buf, 100);
    if (rc > 0) {
        streamer_printf(shell_stream, "Data received:\n");
        for (i = 0; i < rc; ++i) {
            spaces += isspace(buf[i]) ? 1 : 0;
            graph += isgraph(buf[i]) ? 1 : 0;
        }
        if (graph > spaces && (graph + spaces) == rc) {
            streamer_write(shell_stream, buf, rc);
            if (buf[i - 1] != '\r' && buf[i - 1] != '\n') {
                streamer_write(shell_stream, "\n", 1);
            }
        } else {
            for (i = 0; i < rc; ) {
                if (isprint(buf[i])) {
                    streamer_printf(shell_stream, " '%c'", buf[i]);
                } else {
                    streamer_printf(shell_stream, " %02X ", buf[i]);
                }
                ++i;
                if (i % 16 == 0 || i == rc) {
                    streamer_write(shell_stream, "\n", 1);
                }
            }
        }
    }
}

static void
max3107_error(struct max3107_dev *dev, max3107_error_t errcode)
{
    switch (errcode) {
    case MAX3107_ERROR_IO_FAILURE:
        streamer_printf(shell_stream, "SPI read/write error\n");
        break;
    case MAX3107_ERROR_UART_FRAMING:
        streamer_printf(shell_stream, "UART framing error\n");
        break;
    case MAX3107_ERROR_UART_OVERRUN:
        streamer_printf(shell_stream, "UART overrun error\n");
        break;
    case MAX3107_ERROR_UART_PARITY:
        streamer_printf(shell_stream, "UART parity error\n");
        break;
    }
}

static void
max3107_break(struct max3107_dev *dev)
{
    streamer_printf(shell_stream, "Break detected\n");
}

static struct max3107_client max3107_client = {
    .writable = max3107_writable,
    .readable = max3107_readable,
    .error = max3107_error,
    .break_detected = max3107_break,
};

static int
max3107_shell_cmd_close(const struct shell_cmd *cmd, int argc, char **argv, struct streamer *str)
{
    (void)cmd;
    (void)argc;
    (void)argv;
    (void)str;

    if (max3107_dev) {
        os_dev_close((struct os_dev *)&max3107_dev);
    }
    max3107_dev = NULL;

    return 0;
}

static int
max3107_shell_cmd_open(const struct shell_cmd *cmd, int argc, char **argv, struct streamer *str)
{
    const char *name = "max3107_0";

    (void)cmd;

    if (argc > 1) {
        name = argv[1];
    }
    if (max3107_dev) {
        if (strcmp(name, ((struct os_dev *)max3107_dev)->od_name) != 0) {
            os_dev_close((struct os_dev *)max3107_dev);
        }
    }

    shell_stream = str;

    max3107_dev = max3107_open(name, &max3107_client);
    if (max3107_dev == 0) {
        streamer_printf(str, "Failed to open device %s\n", name);
    } else {
        shell_stream = str;
    }

    return 0;
}

static int
max3107_shell_cmd_reg(const struct shell_cmd *cmd, int argc, char **argv, struct streamer *str)
{
    uint8_t reg;
    uint8_t val;
    int rc;

    (void)cmd;

    if (!max3107_dev) {
        streamer_printf(str, "Err: Device not opened. Use open <device> first.\n");
        return 0;
    }
    if (argc < 2) {
        streamer_printf(str, "Err: Register number not specified.\n");
        return 0;
    }
    reg = (uint8_t)parse_ll_bounds(argv[1], 0, 255, &rc);
    if (rc) {
        streamer_printf(str, "Err: Register number out of bounds <0..255>.\n");
        return 0;
    }
    if (argc > 2) {
        val = (uint8_t)parse_ll_bounds(argv[2], 0, 255, &rc);
        if (rc) {
            streamer_printf(str, "Err: Register value out of bounds <0..255>.\n");
            return 0;
        }
        max3107_write_regs(max3107_dev, reg, &val, 1);
    } else {
        rc = max3107_read_regs(max3107_dev, reg, &val, 1);
        if (rc) {
            streamer_printf(str, "Err: Failed to read register %d.\n", rc);
        } else {
            streamer_printf(str, "reg %s 0x%02X\n", argv[1], val);
        }
    }

    return 0;
}

static int
max3107_shell_cmd_write(const struct shell_cmd *cmd, int argc, char **argv, struct streamer *str)
{
    int rc;
    int i;

    (void)cmd;

    if (max3107_dev == NULL) {
        streamer_printf(str, "Err: device not opened yet.\n");
    } else if (argc < 2) {
        streamer_printf(str, "Err: Insufficient arguments\n");
    }
    else {
        for (i = 1; i < argc; ++i) {
            rc = max3107_write(max3107_dev, argv[i], strlen(argv[i]));
            if (rc < 0) {
                streamer_printf(str, "Err: Write failed %d\n", rc);
            }
        }
    }

    return 0;
}

static int
max3107_shell_cmd_dump(const struct shell_cmd *cmd, int argc, char **argv, struct streamer *str)
{
    int rc;
    int i;
    uint8_t val;

    (void)cmd;
    (void)argc;
    (void)argv;

    if (max3107_dev == NULL) {
        streamer_printf(str, "Err: device not opened yet.\n");
        return 0;
    }
    for (i = 1; i <= 0x1F; ++i) {
        rc = max3107_read_regs(max3107_dev, i, &val, 1);
        if (rc < 0) {
            streamer_printf(str, "Err: Read failed %d\n", rc);
        } else {
            streamer_printf(str, "0x%02X = 0x%02X\n", i, val);
        }
    }

    return 0;
}

static const struct shell_cmd max3107_cmds[] = {
    SHELL_CMD_EXT("open", max3107_shell_cmd_open, HELP(open)),
    SHELL_CMD_EXT("close", max3107_shell_cmd_close, HELP(close)),
    SHELL_CMD_EXT("write", max3107_shell_cmd_write, HELP(write)),
    SHELL_CMD_EXT("reg", max3107_shell_cmd_reg, HELP(reg)),
    SHELL_CMD_EXT("dump", max3107_shell_cmd_dump, HELP(dump)),
    SHELL_CMD_EXT(NULL, NULL, NULL)
};

#if MYNEWT_VAL(SHELL_COMPAT)
static const struct shell_cmd max3107_shell_cmd_struct;

static int
max3107_help(void)
{
    console_printf("%s cmd\n", max3107_shell_cmd_struct.sc_cmd);
    console_printf("cmd:\n");
    console_printf("\thelp\n");
    console_printf("\topen\n");
    console_printf("\tclose\n");
    console_printf("\twrite <text>\n");
    console_printf("\treg <reg_num> [<value>]\n");
    console_printf("\tdump\n");
    return 0;
}

static int
max3107_shell_cmd(int argc, char **argv)
{
    const struct shell_cmd *cmd = max3107_cmds;

    argv++;
    argc--;
    if (argc == 0 || strcmp(argv[0], "help") == 0) {
        return max3107_help();
    }

    for (; cmd->sc_cmd; ++cmd) {
        if (strcmp(cmd->sc_cmd, argv[0]) == 0) {
            return cmd->sc_cmd_ext_func(cmd, argc, argv, streamer_console_get());
        }
    }

    console_printf("Unknown command %s\n", argv[1]);
    return 0;
}

static const struct shell_cmd max3107_shell_cmd_struct = {
    .sc_cmd = "max3107",
    .sc_cmd_func = max3107_shell_cmd
};
#endif

void
max3107_shell_init(void)
{
    int rc;

#if MYNEWT_VAL(SHELL_COMPAT)
    rc = shell_cmd_register(&max3107_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

    rc = shell_register("max3107", max3107_cmds);
    SYSINIT_PANIC_ASSERT(rc == 0);
}
