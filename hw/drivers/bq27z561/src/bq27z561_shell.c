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
#include "os/mynewt.h"
#include "bq27z561/bq27z561.h"
#include "console/console.h"
#include "shell/shell.h"

#if MYNEWT_VAL(BQ27Z561_CLI)

static int
bq27z561_std_read_cmd(struct bq27z561 * bq27z561, int argc, char * argv[])
{
    int rc;
    uint8_t reg;
    uint16_t val;

    if (argc != 1) {
        return EINVAL;
    }

    reg = atoi(argv[0]);
    if (reg > BQ27Z561_REG_DCAP) {
        console_printf("Unsupported or invalid regsiter %u\n", reg);
    }
    rc = bq27z561_rd_std_reg_word(bq27z561, reg, &val);
    if (rc) {
        console_printf("Error reading chip\n");
    } else {
        console_printf("Reg %u returned %u (0x%04x)\n", reg, val, val);
    }
    return 0;
}

struct subcmd {
    const char * name;
    const char * help;
    int (*func)(struct bq27z561 * bq27z561, int argc, char * argv[]);
};

static const struct subcmd supported_subcmds[] = {
    {
        .name = "std_read",
        .help = "<cmd>",
        .func = bq27z561_std_read_cmd,
    },
};

static int
bq27z561_shell_cmd(int argc, char * argv[])
{
    struct os_dev * dev;
    struct bq27z561 *bq27;
    const struct subcmd * subcmd;
    uint8_t i;

    dev = os_dev_open(MYNEWT_VAL(BQ27Z561_SHELL_DEV_NAME), OS_TIMEOUT_NEVER,
                      NULL);
    if (dev == NULL) {
        console_printf("failed to open bq27z561_0 device\n");
        return ENODEV;
    }

    bq27 = (struct bq27z561 *)dev;

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
        if (subcmd->func(bq27, argc - 2, argv + 2) != 0) {
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

static const struct shell_cmd bq27z561_shell_cmd_desc = {
    .sc_cmd      = "bq27z561",
    .sc_cmd_func = bq27z561_shell_cmd,
};

int
bq27z561_shell_init(void)
{
    return shell_cmd_register(&bq27z561_shell_cmd_desc);
}

#endif
