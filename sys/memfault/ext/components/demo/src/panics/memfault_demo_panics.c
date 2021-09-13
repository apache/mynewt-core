//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! CLI commands which require integration of the "panic" component.

#include "memfault/demo/cli.h"

#include <stdlib.h>

#include "memfault/core/compiler.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/errors.h"
#include "memfault/core/platform/core.h"
#include "memfault/core/platform/device_info.h"
#include "memfault/core/reboot_tracking.h"
#include "memfault/panics/assert.h"
#include "memfault/panics/coredump.h"
#include "memfault/panics/platform/coredump.h"
#include "memfault_demo_cli_aux_private.h"

MEMFAULT_NO_OPT
static void do_some_work_base(char *argv[]) {
  // An assert that is guaranteed to fail. We perform
  // the check against argv so that the compiler can't
  // perform any optimizations
  MEMFAULT_ASSERT((uint32_t)(uintptr_t)argv == 0xdeadbeef);
}

MEMFAULT_NO_OPT
static void do_some_work1(char *argv[]) {
  do_some_work_base(argv);
}

MEMFAULT_NO_OPT
static void do_some_work2(char *argv[]) {
  do_some_work1(argv);
}

MEMFAULT_NO_OPT
static void do_some_work3(char *argv[]) {
  do_some_work2(argv);
}

MEMFAULT_NO_OPT
static void do_some_work4(char *argv[]) {
  do_some_work3(argv);
}

MEMFAULT_NO_OPT
static void do_some_work5(char *argv[]) {
  do_some_work4(argv);
}

int memfault_demo_cli_cmd_crash(int argc, char *argv[]) {
  int crash_type = 0;

  if (argc >= 2) {
    crash_type = atoi(argv[1]);
  }

  switch (crash_type) {
    case 0:
      MEMFAULT_ASSERT(0);
      break;

    case 1:
      g_bad_func_call();
      break;

    case 2: {
      uint64_t *buf = g_memfault_unaligned_buffer;
      *buf = 0xbadcafe0000;
    } break;

    case 3:
      do_some_work5(argv);
      break;

    case 4:
      MEMFAULT_SOFTWARE_WATCHDOG();
      break;

    default:
      // this should only ever be reached if crash_type is invalid
      MEMFAULT_LOG_ERROR("Usage: \"crash\" or \"crash <n>\" where n is 0..4");
      return -1;
  }

  // Should be unreachable. If we get here, trigger an assert and record the crash_type which
  // failed to trigger a crash
  MEMFAULT_ASSERT_RECORD((uint32_t)crash_type);
  return -1;
}

int memfault_demo_cli_cmd_get_core(MEMFAULT_UNUSED int argc, MEMFAULT_UNUSED char *argv[]) {
  size_t total_size = 0;
  if (!memfault_coredump_has_valid_coredump(&total_size)) {
    MEMFAULT_LOG_INFO("No coredump present!");
    return 0;
  }
  MEMFAULT_LOG_INFO("Has coredump with size: %d", (int)total_size);
  return 0;
}

int memfault_demo_cli_cmd_clear_core(MEMFAULT_UNUSED int argc, MEMFAULT_UNUSED char *argv[]) {
  MEMFAULT_LOG_INFO("Invalidating coredump");
  memfault_platform_coredump_storage_clear();
  return 0;
}
