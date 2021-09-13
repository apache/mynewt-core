//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! CLI command that exercises the MEMFAULT_TRACE_EVENT API, capturing a
//! Trace Event with the error reason set to "MemfaultDemoCli_Error".

#include "memfault/demo/cli.h"

#include "memfault/core/compiler.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/trace_event.h"

int memfault_demo_cli_cmd_trace_event_capture(int argc, MEMFAULT_UNUSED char *argv[]) {
  // For more information on user-defined error reasons, see
  // the MEMFAULT_TRACE_REASON_DEFINE macro in trace_reason_user.h .
  MEMFAULT_TRACE_EVENT_WITH_LOG(MemfaultCli_Test, "Example Trace Event. Num Args %d", argc);
  return 0;
}
