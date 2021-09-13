#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Command definitions for the minimal shell/console implementation.

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MemfaultShellCommand {
  const char *command;
  int (*handler)(int argc, char *argv[]);
  const char *help;
} sMemfaultShellCommand;

extern const sMemfaultShellCommand *const g_memfault_shell_commands;
extern const size_t g_memfault_num_shell_commands;

int memfault_shell_help_handler(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif
