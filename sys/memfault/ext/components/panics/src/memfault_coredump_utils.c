//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Convenience utilities that can be used with coredumps

#include "memfault/panics/coredump.h"

#include "memfault/core/debug_log.h"

bool memfault_coredump_storage_check_size(void) {
  sMfltCoredumpStorageInfo storage_info = { 0 };
  memfault_platform_coredump_storage_get_info(&storage_info);
  const size_t size_needed = memfault_coredump_storage_compute_size_required();
  if (size_needed <= storage_info.size) {
    return true;
  }

  MEMFAULT_LOG_ERROR("Coredump storage is %dB but need %dB",
                     (int)storage_info.size, (int)size_needed);
  return false;
}
