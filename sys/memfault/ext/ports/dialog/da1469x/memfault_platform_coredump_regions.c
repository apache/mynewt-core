//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#include "memfault/config.h"
#include "memfault/core/math.h"
#include "memfault/panics/platform/coredump.h"

#include "sdk_defs.h"

#ifndef MEMFAULT_PLATFORM_COREDUMP_CAPTURE_STACK_ONLY
#define MEMFAULT_PLATFORM_COREDUMP_CAPTURE_STACK_ONLY 1
#endif

#if !MEMFAULT_PLATFORM_COREDUMP_STORAGE_REGIONS_CUSTOM
const sMfltCoredumpRegion *memfault_platform_coredump_get_regions(
  const sCoredumpCrashInfo *crash_info, size_t *num_regions) {
  static sMfltCoredumpRegion s_coredump_regions[1];

  int region_idx = 0;

  // first, capture the stack that was active at the time of crash
  const size_t active_stack_size_to_collect = MEMFAULT_PLATFORM_ACTIVE_STACK_SIZE_TO_COLLECT;
  s_coredump_regions[region_idx++] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(
    crash_info->stack_address,
    memfault_platform_sanitize_address_range(crash_info->stack_address,
                                             active_stack_size_to_collect));

  *num_regions = region_idx;
  return &s_coredump_regions[0];
}
#endif

size_t memfault_platform_sanitize_address_range(void *start_addr, size_t desired_size) {
  const uint32_t ram_start = MEMORY_SYSRAM_BASE;
  const uint32_t ram_end = MEMORY_SYSRAM_END;

  if ((uint32_t)start_addr >= ram_start && (uint32_t)start_addr < ram_end) {
    return MEMFAULT_MIN(desired_size, ram_end - (uint32_t)start_addr);
  }
  return 0;
}
