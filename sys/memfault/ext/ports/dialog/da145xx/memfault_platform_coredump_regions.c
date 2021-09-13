//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#include "memfault/config.h"
#include "memfault/panics/platform/coredump.h"
#include "memfault/core/math.h"

#include "arch_ram.h"

#ifndef MEMFAULT_PLATFORM_COREDUMP_CAPTURE_STACK_ONLY
#define MEMFAULT_PLATFORM_COREDUMP_CAPTURE_STACK_ONLY 1
#endif

#if !MEMFAULT_PLATFORM_COREDUMP_STORAGE_REGIONS_CUSTOM
const sMfltCoredumpRegion *memfault_platform_coredump_get_regions(
    const sCoredumpCrashInfo *crash_info, size_t *num_regions) {
  // Let's collect the callstack at the time of crash
  static sMfltCoredumpRegion s_coredump_regions[1];

#if (MEMFAULT_PLATFORM_COREDUMP_CAPTURE_STACK_ONLY == 1)
  const void *stack_start_addr = crash_info->stack_address;
  // Capture only the interrupt stack. Use only if there is not enough storage to capture all of RAM.
#if defined(__GNUC__)
  extern uint32_t __StackTop;
  const uint32_t stack_top_addr = (uint32_t)&__StackTop;
#elif defined (__CC_ARM)
  extern uint32_t __initial_sp;
  const uint32_t stack_top_addr = (uint32_t)&__initial_sp;
#else
  #error "Unsupported Compiler."
#endif

  s_coredump_regions[0] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(
      stack_start_addr, (uint32_t)stack_top_addr - (uint32_t)stack_start_addr);
#else

  // Capture all of RAM. Recommended: it enables broader post-mortem analyses,
  // but has larger storage requirements.
  s_coredump_regions[0] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(
      (uint32_t *)RAM_1_BASE_ADDR, RAM_END_ADDR - RAM_1_BASE_ADDR);
#endif

    *num_regions = MEMFAULT_ARRAY_SIZE(s_coredump_regions);
    return s_coredump_regions;
}
#endif

size_t memfault_platform_sanitize_address_range(void *start_addr, size_t desired_size) {
  static const struct {
    uint32_t start_addr;
    size_t length;
  } s_mcu_mem_regions[] = {
    #if defined (__DA14531__)
      {.start_addr = RAM_1_BASE_ADDR, .length = RAM_2_BASE_ADDR - RAM_1_BASE_ADDR},
      {.start_addr = RAM_2_BASE_ADDR, .length = RAM_3_BASE_ADDR - RAM_2_BASE_ADDR},
      {.start_addr = RAM_3_BASE_ADDR, .length = RAM_END_ADDR - RAM_3_BASE_ADDR},
    #else
      {.start_addr = RAM_1_BASE_ADDR, .length = RAM_2_BASE_ADDR - RAM_1_BASE_ADDR},
      {.start_addr = RAM_2_BASE_ADDR, .length = RAM_3_BASE_ADDR - RAM_2_BASE_ADDR},
      {.start_addr = RAM_3_BASE_ADDR, .length = RAM_4_BASE_ADDR - RAM_3_BASE_ADDR},
      {.start_addr = RAM_4_BASE_ADDR, .length = RAM_END_ADDR - RAM_4_BASE_ADDR},
    #endif
  };

  for (size_t i = 0; i < MEMFAULT_ARRAY_SIZE(s_mcu_mem_regions); i++) {
    const uint32_t lower_addr = s_mcu_mem_regions[i].start_addr;
    const uint32_t upper_addr = lower_addr + s_mcu_mem_regions[i].length;
    if ((uint32_t)start_addr >= lower_addr && ((uint32_t)start_addr < upper_addr)) {
      return MEMFAULT_MIN(desired_size, upper_addr - (uint32_t)start_addr);
    }
  }

  return 0;
}
