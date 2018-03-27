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
#include <string.h>

#include "os/mynewt.h"
#include "console/console.h"
#include "console/ticks.h"

#include "shell/shell.h"
#include "shell_priv.h"

#define PROMPT_LEN 15
#define SHELL_PROMPT "prompt"

/**
 * Handles the 'ticks' command
 */
int
shell_ticks_cmd(int argc, char **argv)
{
    if (argc > 1) {
        if (!strcmp(argv[1], "on")) {
            console_yes_ticks();
            console_printf(" Console Ticks on\n");
        }
        else if (!strcmp(argv[1],"off")) {
            console_printf(" Console Ticks off\n");
            console_no_ticks();
        }
        return 0;
    }
    console_printf(" Usage: ticks [on|off]\n");
    return 0;
}

#if MYNEWT_VAL(SHELL_CMD_HELP)
static const struct shell_param ticks_params[] = {
    {"on", "turn on"},
    {"off", "turn on"},
    { NULL, NULL}
};

static const struct shell_cmd_help ticks_help = {
   .summary = "shell ticks command",
   .usage = "usage: ticks [on|off]",
   .params = ticks_params,
};
#endif

static const struct shell_cmd prompt_commands[] = {
    {
        .sc_cmd = "ticks",
        .sc_cmd_func = shell_ticks_cmd,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &ticks_help,
#endif
    },
    { NULL, NULL, NULL },
};


void
shell_prompt_register(void)
{
    shell_register(SHELL_PROMPT, prompt_commands);
}
