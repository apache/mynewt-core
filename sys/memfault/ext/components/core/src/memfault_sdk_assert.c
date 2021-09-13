//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#include "memfault/core/sdk_assert.h"

#include <inttypes.h>

#include "memfault/core/compiler.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/platform/core.h"


MEMFAULT_WEAK
void memfault_sdk_assert_func_noreturn(void) {
  while (1) { }
}

void memfault_sdk_assert_func(void) {
  void *return_address;
  MEMFAULT_GET_LR(return_address);

  MEMFAULT_LOG_ERROR("ASSERT! LR: 0x%" PRIx32, (uint32_t)(uintptr_t)return_address);
  memfault_platform_halt_if_debugging();
  memfault_sdk_assert_func_noreturn();
}
