//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! CLI commands used by demo applications to exercise the Memfault SDK

#include "memfault/demo/cli.h"

#include <stdlib.h>

#include "memfault/core/compiler.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/device_info.h"
#include "memfault/core/platform/core.h"
#include "memfault/core/platform/device_info.h"
#include "memfault/core/reboot_reason_types.h"
#include "memfault/core/reboot_tracking.h"

int memfault_demo_cli_cmd_get_device_info(MEMFAULT_UNUSED int argc, MEMFAULT_UNUSED char *argv[]) {
  memfault_device_info_dump();
  return 0;
}

int memfault_demo_cli_cmd_system_reboot(MEMFAULT_UNUSED int argc, MEMFAULT_UNUSED char *argv[]) {
  void *pc;
  MEMFAULT_GET_PC(pc);
  void *lr;
  MEMFAULT_GET_LR(lr);
  sMfltRebootTrackingRegInfo reg_info = {
    .pc = (uint32_t)(uintptr_t)pc,
    .lr = (uint32_t)(uintptr_t)lr,
  };

  memfault_reboot_tracking_mark_reset_imminent(kMfltRebootReason_UserReset, &reg_info);
  memfault_platform_reboot();
  return 0;
}
