#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//!
//! Compile time validity checks run on compact logs:
//!  1) Enables printf() style Wformat checking
//!  2) Verifies that number of args passed is <= the maximum number supported  (15)

#include "memfault/config.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#if MEMFAULT_COMPACT_LOG_ENABLE

#include "memfault/core/compiler.h"
#include "memfault/core/preprocessor.h"

#define MEMFAULT_LOGGING_MAX_SUPPORTED_ARGS 15


static void _run_printf_like_func_check(const char* format, ...)
    MEMFAULT_PRINTF_LIKE_FUNC(1, 2);

//! Mark the function as used to prevent warnings in situations where this header is included but
//! no logging is actually used
MEMFAULT_USED
static void _run_printf_like_func_check(MEMFAULT_UNUSED const char* format, ...) { }

//! Compilation time checks on log formatter
//!
//! - static asserts that argument list does not exceed allowed length.
//! - Runs printf() style format checking (behind a "if (false)" so that
//!   the actual code gets optimized away)
#define MEMFAULT_LOGGING_RUN_COMPILE_TIME_CHECKS(format, ...)                           \
  do {                                                                                  \
  MEMFAULT_STATIC_ASSERT(                                                               \
      MEMFAULT_ARG_COUNT_UP_TO_32(__VA_ARGS__) <= MEMFAULT_LOGGING_MAX_SUPPORTED_ARGS , \
      MEMFAULT_EXPAND_AND_QUOTE(MEMFAULT_ARG_COUNT_UP_TO_32(__VA_ARGS__))               \
      " args > MEMFAULT_LOGGING_MAX_SUPPORTED_ARGS ("                                   \
      MEMFAULT_EXPAND_AND_QUOTE(MEMFAULT_LOGGING_MAX_SUPPORTED_ARGS) ")!");             \
  if (false) {                                                                          \
    _run_printf_like_func_check(format, ## __VA_ARGS__);                                \
  } \
} while (0)

#endif /* MEMFAULT_COMPACT_LOG_ENABLE */

#ifdef __cplusplus
}
#endif
