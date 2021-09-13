#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! All Memfault APIs return:
//!    0 on success
//!  < 0 on error
//!
//! This way a memfault API call can be checked with a simple "if (rv != 0) { }" to decide if
//! a call was successful or not
//!
//! For APIs that need to convey more than just success/failure, an API-specific enum will be
//! defined and mapped to values > 0

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "memfault/core/math.h"

//! @brief
//!
//! Internal error codes used within the Memfault components. An external caller should never need
//! to use these Error codes.
typedef enum MemfaultInternalReturnCode {
  MemfaultInternalReturnCode_Error = -1,
  MemfaultInternalReturnCode_InvalidInput = -2,

  MemfaultReturnCode_PlatformBase = -1000
} MemfaultInternalReturnCode;

//! A convenience macro for mapping a platform specific error code to a reserved range. For
//! example, this could be used to map the error codes returned from calls to a MCUs flash driver
//! implementation. This way, meaningful error information can still be surfaced for diagnostics.
//! The goal here is to allow a way to avoid the following pattern where a userful error code
//! always gets mapped down to one value, i.e
//!
//! int rv = some_platform_specific_api()
//! if (rv != 0) {
//!   return -1;
//! }
//!
//! Note: We mask off the top bit and take the absolute value of the original error code to avoid
//! the chance of an overflow. This should leave enough useful info to assist narrowing down where
//! platform specific errors occurred
#define MEMFAULT_PLATFORM_SPECIFIC_ERROR(e) \
  (MemfaultReturnCode_PlatformBase - ((int32_t)MEMFAULT_ABS(e) & 0x7fffffff))

#ifdef __cplusplus
}
#endif
