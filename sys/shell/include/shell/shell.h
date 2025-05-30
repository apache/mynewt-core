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

#ifndef __SHELL_H__
#define __SHELL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "os/mynewt.h"
#include "os/link_tables.h"
#include "streamer/streamer.h"

struct os_eventq;
struct streamer;
struct shell_cmd;

/** Command IDs in the "shell" newtmgr group. */
#define SHELL_NMGR_OP_EXEC      0

/** @brief Callback called when command is entered.
 *
 *  @param argc Number of parameters passed.
 *  @param argv Array of option strings. First option is always command name.
 *
 * @return 0 in case of success or negative value in case of error.
 */
typedef int (*shell_cmd_func_t)(int argc, char *argv[]);

/**
 * @brief Callback for "extended" shell commands.
 *
 * @param cmd                   The shell command being executed.
 * @param argc                  Number of arguments passed.
 * @param argv                  Array of option strings. First option is always
 *                                  command name.
 * @param streamer              The streamer to write shell output to.
 *
 * @return                      0 on success; SYS_E[...] on failure.
 */
typedef int (*shell_cmd_ext_func_t)(const struct shell_cmd *cmd,
                                    int argc, char *argv[],
                                    struct streamer *streamer);

struct shell_param {
    const char *param_name;
    const char *help;
};

struct shell_cmd_help {
    const char *summary;
    const char *usage;
    const struct shell_param *params;
};

struct shell_cmd {
    uint8_t sc_ext : 1; /* 1 if this is an extended shell comand. */
    union {
        shell_cmd_func_t sc_cmd_func;
        shell_cmd_ext_func_t sc_cmd_ext_func;
    };

    const char *sc_cmd;
    const struct shell_cmd_help *help;
};

struct shell_module {
    const char *name;
    const struct shell_cmd *commands;
};

typedef const struct shell_mod *shell_mod_t;
typedef const struct shell_cmd *shell_cmd_t;

/*
 * Shell module enumeration functions
 */
typedef struct shell_mod_ops {
    /** Returns number of module commands */
    size_t (*count)(shell_mod_t mod);
    /** Return module command by it's index */
    shell_cmd_t (*get)(shell_mod_t mod, size_t ix);
} shell_mod_ops_t;

/*
 * Shell module
 */
struct shell_mod {
    /* Shell module functions */
    const shell_mod_ops_t *ops;
    /* Shell module name visible to end use */
    const char *name;
};

/*
 * Shell module with static array of commands
 */
typedef struct shell_mod_std {
    /* Inherited shell module */
    struct shell_mod mod;
    /* Beginning of command array */
    const struct shell_cmd *commands;
    /* Pointer to end of command array */
    const struct shell_cmd *commands_end;
} shell_mod_std_t;
extern const struct shell_mod_ops shell_mod_std_ops;

static inline int
shell_mod_command_count(shell_mod_t mod)
{
    return mod->ops->count(mod);
}

static inline shell_cmd_t
shell_mod_command(shell_mod_t mod, size_t ix)
{
    return mod->ops->get(mod, ix);
}

LINK_TABLE(shell_mod_t, shell_modules)
/* Section name for shell modules */
#define SHELL_MODULES_SECTION LINK_TABLE_SECTION(shell_modules)
#define SHELL_MODULE_SECTION(mod)                                             \
    LINK_TABLE_ELEMENT_SECTION(shell_modules, mod)

/**
 * Create shell module with commands designated by begin and end pointers
 *
 * This will create object of type shell_mod_std and pointer to this object.
 * Pointer will be placed in .shell_modules section so it will be put
 * together with all other shell modules on one link time table.
 *
 * @param _name - name of the module
 * @param _command_begin - pointer to start of the table with commands
 * @param _command_end - pointer to end of the table with commands
 */
#define SHELL_MODULE_STD(_name, _command_begin, _command_end)                 \
    const shell_mod_std_t shell_module_##_name = {                            \
        { &shell_mod_std_ops, #_name },                                       \
        _command_begin, _command_end                                          \
    };                                                                        \
    LINK_TABLE_ELEMENT_REF(shell_modules, _name, shell_module_##_name.mod)

/**
 * Create shell module with static command table
 *
 * @param _name - name of the module
 * @parma _table - standard table of const struct shell_cmd with know size
 */
#define SHELL_MODULE_WITH_TABLE(name_, table_)                                \
    SHELL_MODULE_STD(name_, table_, table_ + ARRAY_SIZE(table_))

#define SHELL_MODULE_CMD_SECTION(mod_, cmd_)                                  \
    LINK_TABLE_ELEMENT_SECTION(mod_##_commands, cmd_)
/* Section name for shell compat (global) commands */
#define SHELL_COMPAT_CMDS_SECTION LINK_TABLE_SECTION(compat_commands)
/* Section name for shell compat command */
#define SHELL_COMPAT_CMD_SECTION(cmd_) SHELL_MODULE_CMD_SECTION(compat, cmd_)

/**
 * Create shell module with link time table (name derived from _name)
 *
 * Link table will have name derived from shell module.
 * Link table must be declared in some pkg.yml in pkg.link_tables: section
 *
 * i.e.
 * pkg.yml
 *
 * pkg.link_tables:
 *     - mcu_commands
 *
 * mcu_cli.c
 *
 * SHELL_MODULE_WITH_LINK_TABLE(mcu)
 *
 * @param _name - name of the shell module
 */
#define SHELL_MODULE_WITH_LINK_TABLE(_name)                                   \
    LINK_TABLE(struct shell_cmd, _name##_commands)                            \
    SHELL_MODULE_STD(_name, LINK_TABLE_START(_name##_commands),               \
                     LINK_TABLE_END(_name##_commands))

/**
 * Create command for module
 *
 * Macro creates object of struct shell_cmd and puts it in link table
 * for module
 *
 * @param mod_ - module name
 * @param cmd_ - command name
 * @param name_ - command name as string
 * @param ext_ - 1 for extended command with streamer
 * @param func_ - function to execute
 * @param help_ - help structure for function
 */
#define SHELL_MODULE_CMD_WITH_NAME(mod_, cmd_, name_, ext_, func_, help_)            \
    const struct shell_cmd shell_cmd_##cmd_ SHELL_MODULE_CMD_SECTION(mod_, cmd_) = { \
        .sc_ext = ext_,                                                              \
        .sc_cmd_ext_func = (shell_cmd_ext_func_t)(func_),                            \
        .sc_cmd = name_,                                                             \
        .help = SHELL_HELP_(help_),                                                  \
    };

/**
 * Create command for module
 *
 * Macro creates object of struct shell_cmd and puts it in link table
 * for module
 *
 * @param mod_ - module name
 * @param cmd_ - command name
 * @param func_ - function to execute
 * @param help_ - help structure for function
 */
#define SHELL_MODULE_CMD(mod_, cmd_, func_, help_)                            \
    SHELL_MODULE_CMD_WITH_NAME(mod_, cmd_, #cmd_, 0, func_, help_)

/**
 * Create command for module
 *
 * Macro creates object of struct shell_cmd and puts it in link table
 * for module
 *
 * @param mod_ - module name
 * @param cmd_ - command name
 * @param func_ - function to execute
 * @param help_ - help structure for function
 */
#define SHELL_MODULE_EXT_CMD(mod_, cmd_, func_, help_)                        \
    SHELL_MODULE_CMD_WITH_NAME(mod_, cmd_, #cmd_, 1, func_, help_)

/**
 * @brief Register global shell command function
 *
 * Macro creates shell command that can be used when shell task is enabled
 * This is equivalent of calling deprecated function shell_cmd_register.
 *
 * @name - cmd_name command name (it will be put in "" to make string
 * @func - command function function of shell_cmd_ext_func_t
 * @help pointer to shell_cmd_help
 */
#define MAKE_SHELL_CMD(cmd_name, func, help)                                  \
    SHELL_MODULE_CMD(compat, cmd_name, func, help)

/**
 * @brief Register global extended shell command function
 *
 * Macro creates shell command that can be used when shell task is enabled
 * This is equivalent of calling deprecated function shell_cmd_register.
 *
 * @name - cmd_name command name (it will be put in "" to make string
 * @func - command function function of shell_cmd_ext_func_t
 * @help pointer to shell_cmd_help
 */
#define MAKE_SHELL_EXT_CMD(cmd_name, func, help)                              \
    SHELL_MODULE_EXT_CMD(compat, cmd_name, func, help)

#if MYNEWT_VAL(SHELL_CMD_HELP)
#define SHELL_HELP_(help_) (help_)
#else
#define SHELL_HELP_(help_) NULL
#endif

/**
 * @brief constructs a legacy shell command.
 */
#define SHELL_CMD(cmd_, func_, help_) {         \
    .sc_ext = 0,                                \
    .sc_cmd_func = func_,                       \
    .sc_cmd = cmd_,                             \
    .help = SHELL_HELP_(help_),                 \
}

/**
 * @brief constructs an extended shell command.
 */
#define SHELL_CMD_EXT(cmd_, func_, help_) {     \
    .sc_ext = 1,                                \
    .sc_cmd_ext_func = func_,                   \
    .sc_cmd = cmd_,                             \
    .help = SHELL_HELP_(help_),                 \
}

/** @brief Register a shell_module object
 *
 *  @param shell_name Module name to be entered in shell console.
 *
 *  @param shell_commands Array of commands to register.
 *  The array should be terminated with an empty element.
 */
int shell_register(const char *shell_name,
                   const struct shell_cmd *shell_commands);

/** @brief Optionally register an app default cmd handler.
 *
 *  @param handler To be called if no cmd found in cmds registered with
 *  shell_init.
 */
void shell_register_app_cmd_handler(shell_cmd_func_t handler);

/** @brief Callback to get the current prompt.
 *
 *  @returns Current prompt string.
 */
typedef const char *(*shell_prompt_function_t)(void);

/** @brief Optionally register a custom prompt callback.
 *
 *  @param handler To be called to get the current prompt.
 */
void shell_register_prompt_handler(shell_prompt_function_t handler);

/** @brief Optionally register a default module, to avoid typing it in
 *  shell console.
 *
 *  @param name Module name.
 */
void shell_register_default_module(const char *name);

/** @brief Optionally set event queue to process shell command events
 *
 *  @param evq Event queue to be used in shell
 */
void shell_evq_set(struct os_eventq *evq);

/**
 * @brief Processes a set of arguments and executes their corresponding shell
 * command.
 *
 * @param argc                  The argument count (including command name).
 * @param argv                  The argument list ([0] is command name).
 * @param streamer              The streamer to send output to.
 *
 * @return                      0 on success; SYS_E[...] on failure.
 */
int shell_exec(int argc, char **argv, struct streamer *streamer);

#if MYNEWT_VAL(SHELL_MGMT)
struct os_mbuf;
typedef int (*shell_nlip_input_func_t)(struct os_mbuf *, void *arg);
int shell_nlip_input_register(shell_nlip_input_func_t nf, void *arg);
int shell_nlip_output(struct os_mbuf *m);
#endif

#if MYNEWT_VAL(SHELL_COMPAT)
int shell_cmd_register(const struct shell_cmd *sc);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SHELL_H__ */
