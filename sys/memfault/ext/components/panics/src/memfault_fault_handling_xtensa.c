//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Fault handling for Xtensa based architectures

#if defined(__XTENSA__)

#include "memfault/core/compiler.h"
#include "memfault/core/platform/core.h"
#include "memfault/core/reboot_tracking.h"
#include "memfault/panics/arch/xtensa/xtensa.h"
#include "memfault/panics/coredump.h"
#include "memfault/panics/coredump_impl.h"

const sMfltCoredumpRegion *memfault_coredump_get_arch_regions(size_t *num_regions) {
  *num_regions = 0;
  return NULL;
}

static eMemfaultRebootReason s_crash_reason = kMfltRebootReason_Unknown;

void memfault_fault_handler(const sMfltRegState *regs, eMemfaultRebootReason reason) {
  if (s_crash_reason == kMfltRebootReason_Unknown) {
    sMfltRebootTrackingRegInfo info = {
      .pc = regs->pc,
    };
    memfault_reboot_tracking_mark_reset_imminent(reason, &info);
    s_crash_reason = reason;
  }

  sMemfaultCoredumpSaveInfo save_info = {
    .regs = regs,
    .regs_size = sizeof(*regs),
    .trace_reason = s_crash_reason,
  };

  // Check out "Windowed Procedure-Call Protocol" in the Xtensa ISA Reference Manual: Some data is
  // stored "below the stack pointer (since they are infrequently referenced), leaving the limited
  // range of the ISA’s load/store offsets available for more frequently referenced locals."
  //
  // Processor saves callers a0..a3 in the 16 bytes below the "sp"
  // The next 48 bytes beneath that are from a _WindowOverflow12 on exception
  // capturing callers a4 - a15
  //
  // For the windowed ABI, a1 always holds the current "sp":
  //   https://github.com/espressif/esp-idf/blob/v4.0/components/freertos/readme_xtensa.txt#L421-L428
  const uint32_t windowed_abi_spill_size = 64;
  const uint32_t sp_prior_to_exception = regs->a[1] - windowed_abi_spill_size;

  sCoredumpCrashInfo info = {
    .stack_address = (void *)sp_prior_to_exception,
    .trace_reason = save_info.trace_reason,
    .exception_reg_state = regs,
  };
  save_info.regions = memfault_platform_coredump_get_regions(&info, &save_info.num_regions);

  const bool coredump_saved = memfault_coredump_save(&save_info);
  if (coredump_saved) {
    memfault_reboot_tracking_mark_coredump_saved();
  }
}

size_t memfault_coredump_storage_compute_size_required(void) {
  // actual values don't matter since we are just computing the size
  sMfltRegState core_regs = { 0 };
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

#endif /* __XTENSA__ */
