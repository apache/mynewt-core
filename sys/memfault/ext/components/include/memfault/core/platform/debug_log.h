#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! APIs that need to be implemented in order to enable logging within the memfault SDK
//!
//! The memfault SDK uses logs sparingly to communicate useful diagnostic information to the
//! integrator of the library

#include <stddef.h>

#include "memfault/core/compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  kMemfaultPlatformLogLevel_Debug = 0,
  kMemfaultPlatformLogLevel_Info,
  kMemfaultPlatformLogLevel_Warning,
  kMemfaultPlatformLogLevel_Error,
  // Convenience definition to get the number of possible levels
  kMemfaultPlatformLogLevel_NumLevels,
} eMemfaultPlatformLogLevel;

//! Routine for displaying (or capturing) a log.
//!
//! @note it's expected that the implementation will terminate the log with a newline
//! @note Even if there is no UART or RTT Console, it's worth considering adding a logging
//! implementation that writes to RAM or flash which allows for post-mortem analysis
MEMFAULT_PRINTF_LIKE_FUNC(2, 3)
void memfault_platform_log(eMemfaultPlatformLogLevel level, const char *fmt, ...);

//! Routine for printing a log line as-is, only appending a newline, but without suffixing or
//! appending timestamps, log level info, etc. This is used for debug console-commands where
//! it would degrade the developer experience when the string would be suffixed/post-fixed with
//! other characters.
//!
//! @note it's expected that the implementation will terminate the log with a newline
//! but NOT suffix or append anything other than that.
MEMFAULT_PRINTF_LIKE_FUNC(1, 2)
void memfault_platform_log_raw(const char *fmt, ...);

//! Routine for displaying (or capturing) hexdumps
void memfault_platform_hexdump(eMemfaultPlatformLogLevel level, const void *data, size_t data_len);

#ifdef __cplusplus
}
#endif
