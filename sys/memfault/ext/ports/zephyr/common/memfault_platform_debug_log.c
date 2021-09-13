//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Maps memfault platform logging API to zephyr kernel logs

#include "memfault/core/platform/debug_log.h"

#include <stdio.h>

#include "zephyr_release_specific_headers.h"

#include "memfault/config.h"

#ifndef MEMFAULT_DEBUG_LOG_BUFFER_SIZE_BYTES
#  define MEMFAULT_DEBUG_LOG_BUFFER_SIZE_BYTES (128)
#endif

void memfault_platform_log(eMemfaultPlatformLogLevel level, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  char log_buf[MEMFAULT_DEBUG_LOG_BUFFER_SIZE_BYTES];
  vsnprintf(log_buf, sizeof(log_buf), fmt, args);

  const char *lvl_str = "???";
  switch (level) {
    case kMemfaultPlatformLogLevel_Debug:
      lvl_str = "dbg";
      break;

    case kMemfaultPlatformLogLevel_Info:
      lvl_str = "inf";
      break;

    case kMemfaultPlatformLogLevel_Warning:
      lvl_str = "wrn";
      break;

    case kMemfaultPlatformLogLevel_Error:
      lvl_str = "err";
      break;

    default:
      break;
  }
  printk("<%s> <mflt>: %s\n", lvl_str, log_buf);

  va_end(args);
}

void memfault_platform_log_raw(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprintk(fmt, args);
  va_end(args);
  printk("\n");
}
