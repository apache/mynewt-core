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
#include <assert.h>

#include "os/mynewt.h"
#include "console/console.h"
#include "streamer/streamer.h"
#include "modlog/modlog.h"
#include "shell/shell.h"
#include "shell_priv.h"

#define SHELL_PROMPT "shell"

static struct shell_module shell_modules[MYNEWT_VAL(SHELL_MAX_MODULES)];
static size_t num_of_shell_entities;

static const char *prompt;
static int default_module = -1;

static struct shell_cmd app_cmd;
static shell_prompt_function_t app_prompt_handler;

/* Shared queue for shell events to be processed */
static struct os_eventq *shell_evq;
/* Queue for available shell events */
static struct os_event shell_console_ev[MYNEWT_VAL(SHELL_MAX_CMD_QUEUED)];
static struct console_input buf[MYNEWT_VAL(SHELL_MAX_CMD_QUEUED)];

void
shell_evq_set(struct os_eventq *evq)
{
    shell_evq = evq;
    console_line_queue_set(shell_evq);
}

static const char *
get_prompt(void)
{
    const char *str;

    if (app_prompt_handler) {

        str = app_prompt_handler();
        if (str) {
            return str;
        }
    }

    if (default_module != -1) {
        return shell_modules[default_module].name;
    }

    return prompt;
}

static void
print_prompt(void)
{
    console_printf("%s%s", get_prompt(), MYNEWT_VAL(SHELL_PROMPT_SUFFIX));
}

static void
print_prompt_if_console(struct streamer *streamer)
{
    if (streamer == streamer_console_get()) {
        print_prompt();
    }
}

static size_t
line2argv(char *str, char *argv[], size_t size, struct streamer *streamer)
{
    size_t argc = 0;

    if (!strlen(str)) {
        return 0;
    }

    while (*str && *str == ' ') {
        str++;
    }

    if (!*str) {
        return 0;
    }

    argv[argc++] = str;

    while ((str = strchr(str, ' '))) {
        *str++ = '\0';

        while (*str && *str == ' ') {
            str++;
        }

        if (!*str) {
            break;
        }

        argv[argc++] = str;

        if (argc == size) {
            streamer_printf(streamer, "Too many parameters (max %zu)\n",
                            size - 1);
            return 0;
        }
    }

    /* keep it POSIX style where argv[argc] is required to be NULL */
    argv[argc] = NULL;

    return argc;
}

static int
get_destination_module(const char *module_str, int len)
{
    int i;

    for (i = 0; i < num_of_shell_entities; i++) {
        if (len < 0) {
            if (!strcmp(module_str, shell_modules[i].name)) {
                return i;
            }
        } else {
            if (!strncmp(module_str, shell_modules[i].name, len)) {
                return i;
            }
        }
    }

    return -1;
}

/* For a specific command: argv[0] = module name, argv[1] = command name
 * If a default module was selected: argv[0] = command name
 */
static const char *
get_command_and_module(char *argv[], int *module, struct streamer *streamer)
{
    *module = -1;

    if (!argv[0]) {
        streamer_printf(streamer, "Unrecognized command\n");
        return NULL;
    }

    if (default_module == -1) {
        if (!argv[1] || argv[1][0] == '\0') {
            streamer_printf(streamer, "Unrecognized command: %s\n", argv[0]);
            return NULL;
        }

        *module = get_destination_module(argv[0], -1);
        if (*module == -1) {
            streamer_printf(streamer, "Illegal module %s\n", argv[0]);
            return NULL;
        }

        return argv[1];
    }

    *module = default_module;
    return argv[0];
}

static void
print_command_params(const int module, const int command,
                     struct streamer *streamer)
{
	const struct shell_module *shell_module = &shell_modules[module];
	const struct shell_cmd *shell_cmd = &shell_module->commands[command];
	int i;

	if (!(shell_cmd->help && shell_cmd->help->params)) {
		return;
	}

	for (i = 0; shell_cmd->help->params[i].param_name; i++) {
		streamer_printf(streamer, "%-30s%s\n",
                        shell_cmd->help->params[i].param_name,
			            shell_cmd->help->params[i].help);
	}
}

static int
show_cmd_help(char *argv[], struct streamer *streamer)
{
    const char *command = NULL;
    int module = -1;
    const struct shell_module *shell_module = NULL;
    const struct shell_cmd *cmd;
    int i;

    command = get_command_and_module(argv, &module, streamer);
    if ((module == -1) || (command == NULL)) {
        return 0;
    }

    shell_module = &shell_modules[module];
    for (i = 0; shell_module->commands[i].sc_cmd; i++) {
        cmd = &shell_module->commands[i];

        if (!strcmp(command, cmd->sc_cmd)) {
            if (!cmd->help || (!cmd->help->summary &&
                               !cmd->help->usage &&
                               !cmd->help->params)) {
                streamer_printf(streamer, "(no help available)\n");
                return 0;
            }

            if (cmd->help->summary) {
                streamer_printf(streamer, "Summary:\n");
                streamer_printf(streamer, "%s\n", cmd->help->summary);
            }

            if (cmd->help->usage) {
                streamer_printf(streamer, "Usage:\n");
                streamer_printf(streamer, "%s\n", cmd->help->usage);
            }

            if (cmd->help->params) {
                streamer_printf(streamer, "Parameters:\n");
                print_command_params(module, i, streamer);
            }

            return 0;
        }
    }

    streamer_printf(streamer, "Unrecognized command: %s\n", argv[0]);
    return 0;
}

static void
print_modules(struct streamer *streamer)
{
    int module;

    for (module = 0; module < num_of_shell_entities; module++) {
        streamer_printf(streamer, "%s\n", shell_modules[module].name);
    }
}

static void
print_module_commands(const int module, struct streamer *streamer)
{
    const struct shell_module *shell_module = &shell_modules[module];
    int i;

    streamer_printf(streamer, "help\n");

    for (i = 0; shell_module->commands[i].sc_cmd; i++) {
        streamer_printf(streamer, "%-30s", shell_module->commands[i].sc_cmd);
        if (shell_module->commands[i].help &&
            shell_module->commands[i].help->summary) {
        
            streamer_printf(streamer, "%s",
                            shell_module->commands[i].help->summary);
        }
        streamer_printf(streamer, "\n");
    }
}

static int
show_help(const struct shell_cmd *cmd, int argc, char *argv[],
          struct streamer *streamer)
{
    int module;

    /* help per command */
    if ((argc > 2) || ((default_module != -1) && (argc == 2))) {
        return show_cmd_help(&argv[1], streamer);
    }

    /* help per module */
    if ((argc == 2) || ((default_module != -1) && (argc == 1))) {
        if (default_module == -1) {
            module = get_destination_module(argv[1], -1);
            if (module == -1) {
                streamer_printf(streamer, "Illegal module %s\n", argv[1]);
                return 0;
            }
        } else {
            module = default_module;
        }

        print_module_commands(module, streamer);
    } else { /* help for all entities */
        streamer_printf(streamer, "Available modules:\n");
        print_modules(streamer);
        streamer_printf(streamer,
                        "To select a module, enter 'select <module name>'.\n");
    }

    return 0;
}

static const struct shell_cmd shell_cmd_help =
    SHELL_CMD_EXT("help", show_help, NULL);

static int
set_default_module(const char *name)
{
    int module;

    module = get_destination_module(name, -1);

    if (module == -1) {
        return -1;
    }

    default_module = module;

    return 0;
}

static int
select_module(const struct shell_cmd *cmd, int argc, char *argv[],
              struct streamer *streamer)
{
    if (argc == 1) {
        default_module = -1;
    } else {
        set_default_module(argv[1]);
    }

    return 0;
}

static const struct shell_cmd shell_cmd_select_module =
    SHELL_CMD_EXT("select", select_module, NULL);

static const struct shell_cmd *
shell_find_cmd(int argc, char *argv[], struct streamer *streamer)
{
    const char *first_string = argv[0];
    int module = -1;
    const struct shell_module *shell_module;
    const char *command;
    int i;

    if (!first_string || first_string[0] == '\0') {
        streamer_printf(streamer, "Illegal parameter\n");
        return NULL;
    }

    if (!strcmp(first_string, "help")) {
        return &shell_cmd_help;
    }

    if (!strcmp(first_string, "select")) {
        return &shell_cmd_select_module;
    }

    if ((argc == 1) && (default_module == -1)) {
        streamer_printf(streamer, "Missing parameter\n");
        return NULL;
    }

    command = get_command_and_module(argv, &module, streamer);
    if ((module == -1) || (command == NULL)) {
        return NULL;
    }

    shell_module = &shell_modules[module];
    for (i = 0; shell_module->commands[i].sc_cmd; i++) {
        if (!strcmp(command, shell_module->commands[i].sc_cmd)) {
            return &shell_module->commands[i];
        }
    }

    return NULL;
}

int
shell_exec(int argc, char **argv, struct streamer *streamer)
{
    const struct shell_cmd *cmd;
    size_t argc_offset = 0;
    int rc;

    cmd = shell_find_cmd(argc, argv, streamer);
    if (!cmd) {
        if (app_cmd.sc_cmd_func != NULL) {
            cmd = &app_cmd;
        } else {
            streamer_printf(streamer, "Unrecognized command: %s\n", argv[0]);
            streamer_printf(streamer,
                            "Type 'help' for list of available commands\n");
            print_prompt_if_console(streamer);
            return SYS_ENOENT;
        }
    }

    /* Allow invoking a cmd with module name as a prefix; a command should
     * not know how it was invoked (with or without prefix)
     */
    if (default_module == -1 && cmd != &shell_cmd_select_module &&
        cmd != &shell_cmd_help) {
        argc_offset = 1;
    }

    /* Execute callback with arguments */
    if (!cmd->sc_ext) {
        rc = cmd->sc_cmd_func(argc - argc_offset, &argv[argc_offset]);
    } else {
        rc = cmd->sc_cmd_ext_func(cmd, argc - argc_offset, &argv[argc_offset],
                                  streamer);
    }
    if (rc < 0) {
        show_cmd_help(argv, streamer);
    }

    print_prompt_if_console(streamer);

    return rc;
}

static void
shell_process_command(char *line, struct streamer *streamer)
{
    char *argv[MYNEWT_VAL(SHELL_CMD_ARGC_MAX) + 1];
    size_t argc;

    argc = line2argv(line, argv, MYNEWT_VAL(SHELL_CMD_ARGC_MAX) + 1, streamer);
    if (!argc) {
        print_prompt_if_console(streamer);
        return;
    }

    shell_exec(argc, argv, streamer);
}

#if MYNEWT_VAL(SHELL_NEWTMGR)
static void
shell_process_nlip_line(char *shell_line, struct streamer *streamer)
{
    size_t shell_line_len;

    shell_line_len = strlen(shell_line);
    if (shell_line_len > 2) {
        if (shell_line[0] == SHELL_NLIP_PKT_START1 &&
                shell_line[1] == SHELL_NLIP_PKT_START2) {
            shell_nlip_clear_pkt();
            shell_nlip_process(&shell_line[2], shell_line_len - 2);
        } else if (shell_line[0] == SHELL_NLIP_DATA_START1 &&
                shell_line[1] == SHELL_NLIP_DATA_START2) {
            shell_nlip_process(&shell_line[2], shell_line_len - 2);
        } else {
            shell_process_command(shell_line, streamer);
        }
    } else {
        shell_process_command(shell_line, streamer);
    }
}
#endif

static void
shell(struct os_event *ev)
{
    struct console_input *cmd;
    struct streamer *streamer;

    if (!ev) {
        print_prompt();
        return;
    }

    cmd = ev->ev_arg;
    if (!cmd) {
        print_prompt();
        return;
    }

    streamer = streamer_console_get();

#if MYNEWT_VAL(SHELL_NEWTMGR)
    shell_process_nlip_line(cmd->line, streamer);
#else
    shell_process_command(cmd->line, streamer);
#endif

    console_line_event_put(ev);
}

#if MYNEWT_VAL(SHELL_COMPLETION)
static int
get_command_from_module(const char *command, int len, int module)
{
    int i;
    const struct shell_module *shell_module;

    shell_module = &shell_modules[module];
    for (i = 0; shell_module->commands[i].sc_cmd; i++) {

        if (strlen(shell_module->commands[i].sc_cmd) != len) {
            continue;
        }

        if (!strncmp(command, shell_module->commands[i].sc_cmd, len)) {
            return i;
        }
    }
    return -1;
}

static int
get_token(char **cur, int *null_terminated)
{
    char *str = *cur;

    *null_terminated = 0;
    /* remove ' ' at the beginning */
    while (*str && *str == ' ') {
        str++;
    }

    if (*str == '\0') {
        *null_terminated = 1;
        return 0;
    }

    *cur = str;
    str = strchr(str, ' ');

    if (str == NULL) {
        *null_terminated = 1;
        return strlen(*cur);
    }

    return str - *cur;
}

static int
get_last_token(char **cur)
{
    *cur = strrchr(*cur, ' ');
    if (*cur == NULL) {
        return 0;
    }
    (*cur)++;
    return strlen(*cur);
}

static void
complete_param(char *line, const char *param_prefix,
               int param_len, int module_idx, int command_idx,
               console_append_char_cb append_char)
{
    const char *first_match = NULL;
    int i, j, common_chars = -1;
    const struct shell_cmd *command;

    command = &shell_modules[module_idx].commands[command_idx];

    if (!(command->help && command->help->params)) {
        return;
    }

    for (i = 0; command->help->params[i].param_name; i++) {

        if (strncmp(param_prefix,
            command->help->params[i].param_name, param_len)) {
            continue;
        }

        if (!first_match) {
            first_match = command->help->params[i].param_name;
            continue;
        }

        /* more commands match, print first match */
        if (first_match && (common_chars < 0)) {
            console_printf("\n");
            console_printf("%s\n", first_match);
            common_chars = strlen(first_match);
        }

        /* cut common part of matching names */
        for (j = 0; j < common_chars; j++) {
            if (first_match[j] != command->help->params[i].param_name[j]) {
                break;
            }
        }

        common_chars = j;

        console_printf("%s\n", command->help->params[i].param_name);
    }

    /* no match, do nothing */
    if (!first_match) {
        return;
    }

    if (common_chars >= 0) {
        /* multiple match, restore prompt */
        print_prompt();
        console_printf("%s", line);
    } else {
        common_chars = strlen(first_match);
    }

    /* complete common part */
    for (i = param_len; i < common_chars; i++) {
        if (!append_char(line, first_match[i])) {
            return;
        }
    }
}

static void
complete_command(char *line, char *command_prefix,
                 int command_len, int module_idx,
                 console_append_char_cb append_char)
{
    int first_match = -1;
    int match_count = 0;
    int i, j, common_chars = -1;
    const struct shell_cmd *commands;

    commands = shell_modules[module_idx].commands;

    for (i = 0; commands[i].sc_cmd; i++) {
        if (0 != strncmp(command_prefix, commands[i].sc_cmd, command_len)) {
            continue;
        }
        match_count++;

        if (match_count == 1) {
            first_match = i;
            common_chars = strlen(commands[i].sc_cmd);
            continue;
        }

        /*
         * Common chars were already reduced to what was printed,
         * no need to check more.
         */
        if (common_chars <= command_len) {
            continue;
        }
        /* Check how many additional chars are same as first command's */
        for (j = command_len; j < common_chars; j++) {
            if (commands[first_match].sc_cmd[j] != commands[i].sc_cmd[j]) {
                break;
            }
        }
        common_chars = j;
    }

    if (match_count == 0) {
        return;
    }

    /* Additional characters could be appended */
    if (common_chars > command_len) {
        /* complete common part */
        for (i = command_len; i < common_chars; i++) {
            if (!append_char(line, (uint8_t)commands[first_match].sc_cmd[i])) {
                return;
            }
        }
        if (match_count == 1) {
            /* Whole word matched, append space */
            append_char(line, ' ');
        }
        return;
    }

    /*
     * More words match but there is nothing that could be appended
     * list all possible matches.
     */
    console_printf("\n");
    console_printf("%s\n", commands[first_match].sc_cmd);
    for (i = first_match + 1; commands[i].sc_cmd; i++) {
        if (0 == strncmp(command_prefix, commands[i].sc_cmd, command_len)) {
            console_printf("%s\n", commands[i].sc_cmd);
        }
    }
    /* restore prompt */
    print_prompt();
    console_printf("%s", line);
}

static void
complete_module(char *line, char *module_prefix,
                int module_len, console_append_char_cb append_char)
{
    int i, j;
    const char *first_match = NULL;
    int common_chars = -1, space = 0;

    if (!module_len) {
        console_printf("\n");
        for (i = 0; i < num_of_shell_entities; i++) {
            console_printf("%s\n", shell_modules[i].name);
        }
        print_prompt();
        console_printf("%s", line);
        return;
    }

    for (i = 0; i < num_of_shell_entities; i++) {

        if (strncmp(module_prefix,
                     shell_modules[i].name,
                     module_len)) {
            continue;
        }

        if (!first_match) {
            first_match = shell_modules[i].name;
            continue;
        }

        /* more commands match, print first match */
        if (first_match && (common_chars < 0)) {
            console_printf("\n");
            console_printf("%s\n", first_match);
            common_chars = strlen(first_match);
        }

        /* cut common part of matching names */
        for (j = 0; j < common_chars; j++) {
            if (first_match[j] != shell_modules[i].name[j]) {
                break;
            }
        }

        common_chars = j;

        console_printf("%s\n", shell_modules[i].name);
    }

    /* no match, do nothing */
    if (!first_match) {
        return;
    }

    if (common_chars >= 0) {
        /* multiple match, restore prompt */
        print_prompt();
        console_printf("%s", line);
    } else {
        common_chars = strlen(first_match);
        space = 1;
    }

    /* complete common part */
    for (i = module_len; i < common_chars; i++) {
        if (!append_char(line, first_match[i])) {
            return;
        }
    }

    /* for convenience add space after command */
    if (space) {
        append_char(line, ' ');
    }
}

static void
complete_select(char *line, char *cur,
                int tok_len, console_append_char_cb append_char)
{
    int null_terminated = 0;

    cur += tok_len + 1;
    tok_len = get_token(&cur, &null_terminated);
    if (tok_len == 0) {
        if (default_module != -1) {
            return;
        }
        console_printf("\n");
        print_modules(streamer_console_get());
        print_prompt();
        console_printf("%s", line);
        return;
    }

    if (null_terminated) {
        if (default_module == -1) {
            complete_module(line, cur, tok_len, append_char);
        }
    }
}

static void
completion(char *line, console_append_char_cb append_char)
{
    char *cur;
    int tok_len;
    int module, command;
    int null_terminated = 0;

    /*
     * line to completion is not ended by '\0' as the line that gets from
     * os_eventq_get function
     */
    if (!append_char(line, '\0')) {
        return;
    }

    cur = line;
    tok_len = get_token(&cur, &null_terminated);

    /* empty token - print options */
    if (tok_len == 0) {
        console_printf("\n");
        if (default_module == -1) {
            print_modules(streamer_console_get());
        } else {
            print_module_commands(default_module, streamer_console_get());
        }
        print_prompt();
        console_printf("%s", line);
        return;
    }

    /* token can be completed */
    if (null_terminated) {
        if (default_module == -1) {
            complete_module(line, cur, tok_len, append_char);
            return;
        }
        complete_command(line, cur, tok_len,
                         default_module, append_char);
        return;
    }

    if (strncmp("select", cur, tok_len) == 0) {
        complete_select(line, cur, tok_len, append_char);
        return;
    }

    if (default_module != -1) {
        module = default_module;
    } else {
        module = get_destination_module(cur, tok_len);

        if (module == -1) {
            return;
        }

        cur += tok_len + 1;
        tok_len = get_token(&cur, &null_terminated);

        if (tok_len == 0) {
            console_printf("\n");
            print_module_commands(module, streamer_console_get());
            print_prompt();
            console_printf("%s", line);
            return;
        }

        if (null_terminated) {
            complete_command(line, cur, tok_len,
                             module, append_char);
            return;
        }
    }

    command = get_command_from_module(cur, tok_len, module);
    if (command == -1) {
        return;
    }

    cur += tok_len;
    tok_len = get_last_token(&cur);
    if (tok_len == 0) {
        console_printf("\n");
        print_command_params(module, command, streamer_console_get());
        print_prompt();
        console_printf("%s", line);
        return;
    }
    complete_param(line, cur, tok_len,
                   module, command, append_char);
    return;
}
#endif /* MYNEWT_VAL(SHELL_COMPLETION) */

void
shell_register_app_cmd_handler(shell_cmd_func_t handler)
{
    app_cmd.sc_cmd_func = handler;
}

void
shell_register_prompt_handler(shell_prompt_function_t handler)
{
    app_prompt_handler = handler;
}

void
shell_register_default_module(const char *name)
{
    int result = set_default_module(name);

    if (result != -1) {
        console_printf("\n");
        print_prompt();
    }
}

static void
shell_avail_queue_init(void)
{
    int i;

    for (i = 0; i < MYNEWT_VAL(SHELL_MAX_CMD_QUEUED); i++) {
        shell_console_ev[i].ev_cb = shell;
        shell_console_ev[i].ev_arg = &buf[i];
        console_line_event_put(&shell_console_ev[i]);
    }
}

int
shell_register(const char *module_name, const struct shell_cmd *commands)
{
    if (num_of_shell_entities >= MYNEWT_VAL(SHELL_MAX_MODULES)) {
        MODLOG_ERROR(LOG_MODULE_DEFAULT, "Max number of modules reached\n");
        assert(0);
    }

    shell_modules[num_of_shell_entities].name = module_name;
    shell_modules[num_of_shell_entities].commands = commands;
    ++num_of_shell_entities;

    return 0;
}

#if MYNEWT_VAL(SHELL_COMPAT)
#define SHELL_COMPAT_MODULE_NAME "compat"
static struct shell_cmd compat_commands[MYNEWT_VAL(SHELL_MAX_COMPAT_COMMANDS) + 1];
static int num_compat_commands;
static int module_registered;

int
shell_cmd_register(const struct shell_cmd *sc)
{
    int rc;

    if (num_compat_commands >= MYNEWT_VAL(SHELL_MAX_COMPAT_COMMANDS)) {
        MODLOG_ERROR(LOG_MODULE_DEFAULT,
                     "Max number of compat commands reached\n");
        assert(0);
    }

    if (!module_registered) {
        shell_register(SHELL_COMPAT_MODULE_NAME, compat_commands);
        module_registered = 1;

        rc = set_default_module(SHELL_COMPAT_MODULE_NAME);
        if (rc != 0) {
            MODLOG_ERROR(LOG_MODULE_DEFAULT,
                         "Illegal module %s, default is not changed\n",
                         SHELL_COMPAT_MODULE_NAME);
        }
    }

    compat_commands[num_compat_commands] = *sc;
    ++num_compat_commands;
    return 0;
}
#endif

void
shell_init(void)
{
    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

#if !MYNEWT_VAL(SHELL_TASK)
    return;
#endif

    shell_avail_queue_init();
    shell_evq_set(os_eventq_dflt_get());

    prompt = SHELL_PROMPT;

#if MYNEWT_VAL(SHELL_NEWTMGR)
    shell_nlip_init();
#endif

#if MYNEWT_VAL(SHELL_COMPLETION)
    console_set_completion_cb(completion);
#endif

#if MYNEWT_VAL(SHELL_OS_MODULE)
    shell_os_register();
#endif
#if MYNEWT_VAL(SHELL_PROMPT_MODULE)
    shell_prompt_register();
#endif
#if MYNEWT_VAL(SHELL_BRIDGE)
    shell_bridge_init();
#endif
}
