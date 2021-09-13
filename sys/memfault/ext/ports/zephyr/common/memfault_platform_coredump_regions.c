//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! The default regions to collect on Zephyr when a crash takes place.
//! Function is defined as weak so an end user can override it.

#include "memfault/panics/platform/coredump.h"
#include "memfault/panics/arch/arm/cortex_m.h"

#include <zephyr.h>
#include <kernel.h>
#include <kernel_structs.h>

#include "memfault/core/compiler.h"
#include "memfault/core/math.h"
#include "memfault/ports/zephyr/coredump.h"

static sMfltCoredumpRegion s_coredump_regions[
    MEMFAULT_COREDUMP_MAX_TASK_REGIONS
    + 2 /* active stack(s) */
    + 1 /* _kernel variable */
#if CONFIG_MEMFAULT_COREDUMP_COLLECT_DATA_REGIONS
    + 1
#endif
#if CONFIG_MEMFAULT_COREDUMP_COLLECT_BSS_REGIONS
    + 1
#endif
    ];

MEMFAULT_WEAK
const sMfltCoredumpRegion *memfault_platform_coredump_get_regions(
    const sCoredumpCrashInfo *crash_info, size_t *num_regions) {

  const bool msp_was_active = (crash_info->exception_reg_state->exc_return & (1 << 3)) == 0;
  int region_idx = 0;

  size_t stack_size_to_collect = memfault_platform_sanitize_address_range(
        crash_info->stack_address, CONFIG_MEMFAULT_COREDUMP_STACK_SIZE_TO_COLLECT);

  s_coredump_regions[region_idx] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(
      crash_info->stack_address, stack_size_to_collect);
  region_idx++;

  if (msp_was_active) {
    // System crashed in an ISR but the running task state is on PSP so grab that too
    void *psp = (void *)(uintptr_t)__get_PSP();

    // Collect a little bit more stack for the PSP since there is an
    // exception frame that will have been stacked on it as well
    const uint32_t extra_stack_bytes = 128;
    stack_size_to_collect = memfault_platform_sanitize_address_range(
        psp, CONFIG_MEMFAULT_COREDUMP_STACK_SIZE_TO_COLLECT + extra_stack_bytes);
    s_coredump_regions[region_idx] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(
        psp, stack_size_to_collect);
    region_idx++;
  }

  s_coredump_regions[region_idx] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(
      &_kernel, sizeof(_kernel));
  region_idx++;

  region_idx += memfault_zephyr_get_task_regions(
      &s_coredump_regions[region_idx],
      MEMFAULT_ARRAY_SIZE(s_coredump_regions) - region_idx);

  //
  // Now that we have captured all the task state, we will
  // fill whatever space remains in coredump storage with the
  // data and bss we can collect!
  //

#if CONFIG_MEMFAULT_COREDUMP_COLLECT_DATA_REGIONS
  region_idx +=  memfault_zephyr_get_data_regions(
      &s_coredump_regions[region_idx], MEMFAULT_ARRAY_SIZE(s_coredump_regions) - region_idx);
#endif

#if CONFIG_MEMFAULT_COREDUMP_COLLECT_BSS_REGIONS
  region_idx +=  memfault_zephyr_get_bss_regions(
      &s_coredump_regions[region_idx], MEMFAULT_ARRAY_SIZE(s_coredump_regions) - region_idx);
#endif

  *num_regions = region_idx;
  return &s_coredump_regions[0];
}
