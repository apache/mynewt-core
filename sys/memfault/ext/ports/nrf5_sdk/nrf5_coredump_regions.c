//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Implements APIs for collecting RAM regions on nRF5 as part of a coredump
//!
//! Options (To override, update memfault_platform_config.h)
//! MEMFAULT_PLATFORM_COREDUMP_CUSTOM_REGIONS (default = 0)
//!  When set to 1, user must define memfault_platform_coredump_get_regions() and
//!  declare their own regions to collect.
//!
//!  MEMFAULT_PLATFORM_COREDUMP_CAPTURE_STACK_ONLY (default = 1)
//!   This mode will collect just the stack that was active at the time of crash
//!   The regions to be captured by adding the following to the projects .ld file:
//!
//!     __MemfaultCoredumpRamStart = ORIGIN(RAM);
//!     __MfltCoredumpRamEnd = ORIGIN(RAM) + LENGTH(RAM);
//!

#include "memfault/panics/platform/coredump.h"
#include "memfault/core/math.h"
#include "sdk_common.h"

#ifndef MEMFAULT_PLATFORM_COREDUMP_CAPTURE_STACK_ONLY
#define MEMFAULT_PLATFORM_COREDUMP_CAPTURE_STACK_ONLY 1
#endif

#ifndef MEMFAULT_PLATFORM_COREDUMP_CUSTOM_REGIONS
#define MEMFAULT_PLATFORM_COREDUMP_CUSTOM_REGIONS 0
#endif

#ifndef MEMFAULT_COREDUMP_RAM_REGION_START_ADDR
extern uint32_t __MemfaultCoredumpRamStart[];
#define MEMFAULT_COREDUMP_STORAGE_START_ADDR ((uint32_t)__MemfaultCoreStorageStart)
#endif

#ifndef MEMFAULT_COREDUMP_RAM_REGION_END_ADDR
extern uint32_t __MfltCoredumpRamEnd[];
#define MEMFAULT_COREDUMP_STORAGE_END_ADDR ((uint32_t)__MemfaultCoreStorageEnd)
#endif

//! Truncates the region if it's outside the bounds of RAM
size_t memfault_platform_sanitize_address_range(void *start_addr, size_t desired_size) {
  // All NRF MCUs RAM starts at this address. No define is exposed in the SDK for it however
  const uint32_t ram_start = 0x20000000;

#ifdef NRF51
  const uint32_t ram_size = (NRF_FICR->SIZERAMBLOCKS) * NRF_FICR->NUMRAMBLOCK;
#else
  const uint32_t ram_size = NRF_FICR->INFO.RAM * 1024;
#endif

  const uint32_t ram_end = ram_start + ram_size;

  if ((uint32_t)start_addr >= ram_start && (uint32_t)start_addr < ram_end) {
    return MEMFAULT_MIN(desired_size, ram_end - (uint32_t)start_addr);
  }
  return 0;
}

#if !MEMFAULT_PLATFORM_COREDUMP_CUSTOM_REGIONS
const sMfltCoredumpRegion *memfault_platform_coredump_get_regions(const sCoredumpCrashInfo *crash_info,
                                                                  size_t *num_regions) {
  // Let's collect the callstack at the time of crash
  static sMfltCoredumpRegion s_coredump_regions[1];

#if (MEMFAULT_PLATFORM_COREDUMP_CAPTURE_STACK_ONLY == 1)
  const void *stack_start_addr = crash_info->stack_address;
  // Capture only the interrupt stack. Use only if there is not enough storage to capture all of RAM.
  s_coredump_regions[0] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(stack_start_addr, (uint32_t)STACK_TOP - (uint32_t)stack_start_addr);
#else
  // Capture all of RAM. Recommended: it enables broader post-mortem analyses,
  // but has larger storage requirements.
  s_coredump_regions[0] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(
    MEMFAULT_COREDUMP_RAM_REGION_START_ADDR,
    MEMFAULT_COREDUMP_RAM_REGION_END_ADDR - MEMFAULT_COREDUMP_RAM_REGION_START_ADDR);
#endif

  *num_regions = MEMFAULT_ARRAY_SIZE(s_coredump_regions);
  return s_coredump_regions;
}
#endif
