//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#include <shell/shell.h>

#include "memfault/core/debug_log.h"

#include "memfault/nrfconnect_port/fota.h"

static int prv_mflt_fota(const struct shell *shell, size_t argc, char **argv) {
#if CONFIG_MEMFAULT_FOTA_CLI_CMD
  MEMFAULT_LOG_INFO("Checking for FOTA");
  const int rv = memfault_fota_start();
  if (rv == 0) {
    MEMFAULT_LOG_INFO("FW is up to date!");
  }
  return rv;
#else
  shell_print(shell, "CONFIG_MEMFAULT_FOTA_CLI_CMD not enabled");
  return -1;
#endif
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_memfault_nrf_cmds,
    SHELL_CMD(fota, NULL, "Perform a FOTA using Memfault client", prv_mflt_fota),
    SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(mflt_nrf, &sub_memfault_nrf_cmds, "Memfault nRF Connect SDK Test Commands", NULL);
