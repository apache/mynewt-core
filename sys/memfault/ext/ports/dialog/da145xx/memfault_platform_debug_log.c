//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Map memfault logging dependencies to da145xx arch_printf implementation.

#include "memfault/core/platform/debug_log.h"

#include "memfault/config.h"

#include <stdarg.h>
#include <stdio.h>

#ifdef CFG_PRINTF
#include "arch_console.h"
#endif

#ifndef MEMFAULT_DEBUG_LOG_BUFFER_SIZE_BYTES
  #define MEMFAULT_DEBUG_LOG_BUFFER_SIZE_BYTES (128)
#endif

void memfault_platform_log(eMemfaultPlatformLogLevel level, const char *fmt, ...) {
#ifdef CFG_PRINTF
  va_list args;
  va_start(args, fmt);

  char log_buf[MEMFAULT_DEBUG_LOG_BUFFER_SIZE_BYTES];
  vsnprintf(log_buf, sizeof(log_buf), fmt, args);

  const char *lvl_str = NULL;
  switch (level) {
    case kMemfaultPlatformLogLevel_Debug:
      lvl_str = "D";
      break;

    case kMemfaultPlatformLogLevel_Info:
      lvl_str = "I";
      break;

    case kMemfaultPlatformLogLevel_Warning:
      lvl_str = "W";
      break;

    case kMemfaultPlatformLogLevel_Error:
      lvl_str = "E";
      break;

    default:
      break;
  }

  if (lvl_str) {
    arch_printf("[%s] MFLT: %s\r\n", lvl_str, log_buf);
  }
  va_end(args);
#endif
}

void memfault_platform_log_raw(const char *fmt, ...) {
#ifdef CFG_PRINTF
  va_list args;
  va_start(args, fmt);

  arch_vprintf(fmt, args);
  arch_printf("\r\n");

  va_end(args);
#endif
}
