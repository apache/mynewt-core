#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Minimal shell/console implementation for platforms that do not include one.
//! NOTE: For simplicity, ANSI escape sequences are not dealt with!

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MemfaultShellImpl {
  //! Function to call whenever a character needs to be sent out.
  int (*send_char)(char c);
} sMemfaultShellImpl;

//! Initializes the demo shell. To be called early at boot.
void memfault_demo_shell_boot(const sMemfaultShellImpl *impl);

//! Call this when a character is received. The character is processed synchronously.
void memfault_demo_shell_receive_char(char c);

#ifdef __cplusplus
}
#endif
