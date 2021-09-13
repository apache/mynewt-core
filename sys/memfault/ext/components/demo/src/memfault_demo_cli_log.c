//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! CLI commands that exercises the MEMFAULT_LOG_... and
//! memfault_log_trigger_collection() APIs. See debug_log.h and log.h for more info.

#include "memfault/demo/cli.h"

#include "memfault/core/compiler.h"
#include "memfault/core/log.h"

int memfault_demo_cli_cmd_test_log(MEMFAULT_UNUSED int argc,
                                   MEMFAULT_UNUSED char *argv[]) {
  MEMFAULT_LOG_SAVE(kMemfaultPlatformLogLevel_Debug, "Debug log!");
  MEMFAULT_LOG_SAVE(kMemfaultPlatformLogLevel_Info, "Info log!");
  MEMFAULT_LOG_SAVE(kMemfaultPlatformLogLevel_Warning, "Warning log!");
  MEMFAULT_LOG_SAVE(kMemfaultPlatformLogLevel_Error, "Error log!");
  return 0;
}

int memfault_demo_cli_cmd_trigger_logs(MEMFAULT_UNUSED int argc,
                                       MEMFAULT_UNUSED char *argv[]) {
  memfault_log_trigger_collection();
  return 0;
}
