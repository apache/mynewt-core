//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! A port for recovering reset reason information by reading the
//! "Reset Reason" (RESETREAS) Register.
//!
//! More details can be found in the "Reset Reason" (RESETREAS)"
//! section of the nRF528xx product specification document for you
//! specific chip.
#include "nrf_power.h"
#include "nrf_sdh.h"
#include "nrf_soc.h"
#include "nrf_stack_guard.h"

#include "memfault/core/debug_log.h"
#include "memfault/core/reboot_reason_types.h"
#include "memfault/core/sdk_assert.h"
#include "memfault/ports/reboot_reason.h"

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>

#ifndef MEMFAULT_ENABLE_REBOOT_DIAG_DUMP
#define MEMFAULT_ENABLE_REBOOT_DIAG_DUMP 1
#endif

#if MEMFAULT_ENABLE_REBOOT_DIAG_DUMP
#define MEMFAULT_PRINT_RESET_INFO(...) MEMFAULT_LOG_INFO(__VA_ARGS__)
#else
#define MEMFAULT_PRINT_RESET_INFO(...)
#endif

// Private helper functions deal with the details of the soft device
// modality.
static uint32_t prv_reset_reason_get(void) {
  if (nrf_sdh_is_enabled()) {
    uint32_t reset_reas = 0;
    sd_power_reset_reason_get(&reset_reas);
    return reset_reas;
  }

  return NRF_POWER->RESETREAS;
}

static void prv_reset_reason_clear(uint32_t reset_reas_clear_mask) {
  if (nrf_sdh_is_enabled()) {
    sd_power_reset_reason_clr(reset_reas_clear_mask);
  } else {
    NRF_POWER->RESETREAS |= reset_reas_clear_mask;
  }
}

// Called by the user application.
void memfault_platform_reboot_tracking_boot(void) {
  // NOTE: Since the NRF52 SDK .ld files are based on the CMSIS ARM Cortex-M linker scripts, we use
  // the bottom of the main stack to hold the 64 byte reboot reason.
  //
  // For a detailed explanation about reboot reason storage options check out the guide at:
  //    https://mflt.io/reboot-reason-storage

  uint32_t reboot_tracking_start_addr = (uint32_t) STACK_BASE;
#if NRF_STACK_GUARD_ENABLED
  reboot_tracking_start_addr += STACK_GUARD_SIZE;
#endif

  sResetBootupInfo reset_reason = {0};
  memfault_reboot_reason_get(&reset_reason);
  memfault_reboot_tracking_boot((void *)reboot_tracking_start_addr, &reset_reason);
}

// Map chip-specific reset reasons to Memfault reboot reasons.
void memfault_reboot_reason_get(sResetBootupInfo *info) {
  MEMFAULT_SDK_ASSERT(info != NULL);

  // Consume the reset reason register leaving it cleared in HW.
  // RESETREAS is part of the always on power domain so it's sticky until a full reset occurs
  // Therefore, we clear the bits which were set so that don't get logged in future reboots as well
  const uint32_t reset_reason_reg = prv_reset_reason_get();
  prv_reset_reason_clear(reset_reason_reg);

  MEMFAULT_PRINT_RESET_INFO("Reset Reason, RESETREAS=0x%" PRIx32, reset_reason_reg);
  MEMFAULT_PRINT_RESET_INFO("Reset Causes: ");

  // Assume "no bits set" implies POR.
  eMemfaultRebootReason reset_reason = kMfltRebootReason_PowerOnReset;

  // These appear to be common. Also, the assumption is
  // that only one bit per actual reset event can be set.
  if (reset_reason_reg & NRF_POWER_RESETREAS_RESETPIN_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Pin Reset");
    reset_reason = kMfltRebootReason_PinReset;
  } else if (reset_reason_reg & NRF_POWER_RESETREAS_DOG_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Watchdog");
    reset_reason = kMfltRebootReason_HardwareWatchdog;
  } else if (reset_reason_reg & NRF_POWER_RESETREAS_SREQ_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Software");
    reset_reason = kMfltRebootReason_SoftwareReset;
  } else if (reset_reason_reg & NRF_POWER_RESETREAS_LOCKUP_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Lockup");
    reset_reason = kMfltRebootReason_SoftwareReset;
  } else if (reset_reason_reg & NRF_POWER_RESETREAS_OFF_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" GPIO Wakeup");
    reset_reason = kMfltRebootReason_DeepSleep;
  } else if (reset_reason_reg & NRF_POWER_RESETREAS_DIF_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Debug Interface Wakeup");
    reset_reason = kMfltRebootReason_DeepSleep;
  }

  // The following are decided based on some #define logic provided by Nordic
  // whereby they use the _Msk suffixed #defines to create (or not) an entry
  // in an enum with an NRF_ prefix and a _MASK suffix.
  // E.g. POWER_X_Msk --> NRF_POWER_X_MASK.
#if defined(POWER_RESETREAS_LPCOMP_Msk)
  else if (reset_reason_reg & NRF_POWER_RESETREAS_LPCOMP_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" LPCOMP Wakeup");
    reset_reason = kMfltRebootReason_DeepSleep;
  }
#endif

#if defined(POWER_RESETREAS_NFC_Msk)
  else if (reset_reason_reg & NRF_POWER_RESETREAS_NFC_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" NFC Wakeup");
    reset_reason = kMfltRebootReason_DeepSleep;
  }
#endif

#if defined(POWER_RESETREAS_VBUS_Msk)
  else if (reset_reason_reg & NRF_POWER_RESETREAS_VBUS_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" VBUS Wakeup");
    reset_reason = kMfltRebootReason_DeepSleep;
  }
#endif

  *info = (sResetBootupInfo) {
    .reset_reason_reg = reset_reason_reg,
    .reset_reason     = reset_reason,
  };
}
