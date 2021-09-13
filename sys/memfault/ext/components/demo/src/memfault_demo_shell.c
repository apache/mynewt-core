//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Minimal shell/console implementation for platforms that do not include one.
//! NOTE: For simplicity, ANSI escape sequences are not dealt with!

#include "memfault/demo/shell.h"
#include "memfault/demo/shell_commands.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "memfault/config.h"
#include "memfault/core/compiler.h"

#define MEMFAULT_SHELL_MAX_ARGS (16)
#define MEMFAULT_SHELL_PROMPT "mflt> "

#define MEMFAULT_SHELL_FOR_EACH_COMMAND(command) \
  for (const sMemfaultShellCommand *command = g_memfault_shell_commands; \
    command < &g_memfault_shell_commands[g_memfault_num_shell_commands]; \
    ++command)

static struct MemfaultShellContext {
  int (*send_char)(char c);
  size_t rx_size;
  char rx_buffer[MEMFAULT_DEMO_SHELL_RX_BUFFER_SIZE];
} s_mflt_shell;

static bool prv_booted(void) {
  return s_mflt_shell.send_char != NULL;
}

static void prv_send_char(char c) {
  if (!prv_booted()) {
    return;
  }
  s_mflt_shell.send_char(c);
}

static void prv_echo(char c) {
  if ('\n' == c) {
    prv_send_char('\r');
    prv_send_char('\n');
  } else if ('\b' == c) {
    prv_send_char('\b');
    prv_send_char(' ');
    prv_send_char('\b');
  } else {
    prv_send_char(c);
  }
}

static char prv_last_char(void) {
  return s_mflt_shell.rx_buffer[s_mflt_shell.rx_size - 1];
}

static bool prv_is_rx_buffer_full(void) {
  return s_mflt_shell.rx_size >= MEMFAULT_DEMO_SHELL_RX_BUFFER_SIZE;
}

static void prv_reset_rx_buffer(void) {
  memset(s_mflt_shell.rx_buffer, 0, sizeof(s_mflt_shell.rx_buffer));
  s_mflt_shell.rx_size = 0;
}

static void prv_echo_str(const char *str) {
  for (const char *c = str; *c != '\0'; ++c) {
    prv_echo(*c);
  }
}

static void prv_send_prompt(void) {
  prv_echo_str(MEMFAULT_SHELL_PROMPT);
}

static const sMemfaultShellCommand *prv_find_command(const char *name) {
  MEMFAULT_SHELL_FOR_EACH_COMMAND(command) {
    if (strcmp(command->command, name) == 0) {
      return command;
    }
  }
  return NULL;
}

static void prv_process(void) {
  if (prv_last_char() != '\n' && !prv_is_rx_buffer_full()) {
    return;
  }

  char *argv[MEMFAULT_SHELL_MAX_ARGS] = {0};
  int argc = 0;

  char *next_arg = NULL;
  for (size_t i = 0; i < s_mflt_shell.rx_size && argc < MEMFAULT_SHELL_MAX_ARGS; ++i) {
    char *const c = &s_mflt_shell.rx_buffer[i];
    if (*c == ' ' || *c == '\n' || i == s_mflt_shell.rx_size - 1) {
      *c = '\0';
      if (next_arg) {
        argv[argc++] = next_arg;
        next_arg = NULL;
      }
    } else if (!next_arg) {
      next_arg = c;
    }
  }

  if (s_mflt_shell.rx_size == MEMFAULT_DEMO_SHELL_RX_BUFFER_SIZE) {
    prv_echo('\n');
  }

  if (argc >= 1) {
    const sMemfaultShellCommand *command = prv_find_command(argv[0]);
    if (!command) {
      prv_echo_str("Unknown command: ");
      prv_echo_str(argv[0]);
      prv_echo('\n');
      prv_echo_str("Type 'help' to list all commands\n");
    } else {
      command->handler(argc, argv);
    }
  }
  prv_reset_rx_buffer();
  prv_send_prompt();
}

void memfault_demo_shell_boot(const sMemfaultShellImpl *impl) {
  s_mflt_shell.send_char = impl->send_char;
  prv_reset_rx_buffer();
  prv_echo_str("\n" MEMFAULT_SHELL_PROMPT);
}

void memfault_demo_shell_receive_char(char c) {
  if (c == '\r' || prv_is_rx_buffer_full() || !prv_booted()) {
    return;
  }

  const bool is_backspace = (c == '\b');
  if (is_backspace && s_mflt_shell.rx_size == 0) {
    return; // nothing left to delete so don't echo the backspace
  }

  prv_echo(c);

  if (is_backspace) {
    s_mflt_shell.rx_buffer[--s_mflt_shell.rx_size] = '\0';
    return;
  }

  s_mflt_shell.rx_buffer[s_mflt_shell.rx_size++] = c;

  prv_process();
}

int memfault_shell_help_handler(MEMFAULT_UNUSED int argc, MEMFAULT_UNUSED char *argv[]) {
  MEMFAULT_SHELL_FOR_EACH_COMMAND(command) {
    prv_echo_str(command->command);
    prv_echo_str(": ");
    prv_echo_str(command->help);
    prv_echo('\n');
  }
  return 0;
}
