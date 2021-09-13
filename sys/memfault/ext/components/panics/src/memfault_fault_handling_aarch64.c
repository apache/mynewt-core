//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Fault handling for AARCH64 based devices

#if defined(__aarch64__)

#include "memfault/core/platform/core.h"
#include "memfault/core/reboot_tracking.h"
#include "memfault/panics/arch/arm/aarch64.h"
#include "memfault/panics/coredump.h"
#include "memfault/panics/coredump_impl.h"
#include "memfault/panics/fault_handling.h"

MEMFAULT_WEAK
void memfault_platform_fault_handler(MEMFAULT_UNUSED const sMfltRegState *regs,
                                     MEMFAULT_UNUSED eMemfaultRebootReason reason) {}

const sMfltCoredumpRegion *memfault_coredump_get_arch_regions(size_t *num_regions) {
  *num_regions = 0;
  return NULL;
}

static eMemfaultRebootReason s_crash_reason = kMfltRebootReason_Unknown;

MEMFAULT_NORETURN
void memfault_fault_handler(const sMfltRegState *regs, eMemfaultRebootReason reason) {
  memfault_platform_fault_handler(regs, reason);

  if (s_crash_reason == kMfltRebootReason_Unknown) {
    sMfltRebootTrackingRegInfo info = {
      .pc = regs->pc,
      .lr = regs->x[30],
    };
    memfault_reboot_tracking_mark_reset_imminent(reason, &info);
    s_crash_reason = reason;
  }

  sMemfaultCoredumpSaveInfo save_info = {
    .regs = regs,
    .regs_size = sizeof(*regs),
    .trace_reason = s_crash_reason,
  };

  sCoredumpCrashInfo info = {
    .stack_address = (void *)regs->sp,
    .trace_reason = save_info.trace_reason,
    .exception_reg_state = regs,
  };

  save_info.regions = memfault_platform_coredump_get_regions(&info, &save_info.num_regions);
  const bool coredump_saved = memfault_coredump_save(&save_info);
  if (coredump_saved) {
    memfault_reboot_tracking_mark_coredump_saved();
  }

  memfault_platform_reboot();
  MEMFAULT_UNREACHABLE;
}

MEMFAULT_NORETURN
static void prv_fault_handling_assert(void *pc, void *lr, eMemfaultRebootReason reason) {
  sMfltRebootTrackingRegInfo info = {
    .pc = (uint32_t)(uintptr_t)pc,
    .lr = (uint32_t)(uintptr_t)lr,
  };
  s_crash_reason = reason;
  memfault_reboot_tracking_mark_reset_imminent(s_crash_reason, &info);

  // For assert path, we will trap into fault handler
  __builtin_trap();
}

void memfault_fault_handling_assert(void *pc, void *lr) {
  prv_fault_handling_assert(pc, lr, kMfltRebootReason_Assert);
}

void memfault_fault_handling_assert_extra(void *pc, void *lr, sMemfaultAssertInfo *extra_info) {
  prv_fault_handling_assert(pc, lr, extra_info->assert_reason);
}

size_t memfault_coredump_storage_compute_size_required(void) {
  // actual values don't matter since we are just computing the size
  sMfltRegState core_regs = {0};
  sMemfaultCoredumpSaveInfo save_info = {
    .regs = &core_regs,
    .regs_size = sizeof(core_regs),
    .trace_reason = kMfltRebootReason_UnknownError,
  };

  sCoredumpCrashInfo info = {
    // we'll just pass the current stack pointer, value shouldn't matter
    .stack_address = (void *)&core_regs,
    .trace_reason = save_info.trace_reason,
    .exception_reg_state = NULL,
  };
  save_info.regions = memfault_platform_coredump_get_regions(&info, &save_info.num_regions);

  return memfault_coredump_get_save_size(&save_info);
}

#endif /* __aarch64__ */
