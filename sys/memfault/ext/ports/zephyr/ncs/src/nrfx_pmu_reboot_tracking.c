//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! A port for recovering reset reason information by reading the
//! "Power management unit" (PMU)'s "reset reason" (RESETREAS) Register.
//!
//! More details can be found in the "RESETREAS" section in an NRF reference manual

#include "memfault/ports/reboot_reason.h"

#include "memfault/config.h"
#include "memfault/core/compiler.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/reboot_reason_types.h"
#include "memfault/core/sdk_assert.h"

#include <nrfx.h>
#include <hal/nrf_power.h>

//! Note: Nordic uses different headers for reset reason information depending on platform. NRF52 &
//! NRF91 reasons are defined in nrf_power.h whereas nrf53 reasons are defined in nrf_reset.h
#if !NRF_POWER_HAS_RESETREAS
#include <hal/nrf_reset.h>
#endif

#if MEMFAULT_ENABLE_REBOOT_DIAG_DUMP
#define MEMFAULT_PRINT_RESET_INFO(...) MEMFAULT_LOG_INFO(__VA_ARGS__)
#else
#define MEMFAULT_PRINT_RESET_INFO(...)
#endif

#if NRF_POWER_HAS_RESETREAS
static eMemfaultRebootReason prv_decode_power_resetreas(uint32_t reset_cause) {
  eMemfaultRebootReason reset_reason;
  if (reset_cause & NRF_POWER_RESETREAS_RESETPIN_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Pin Reset");
    reset_reason = kMfltRebootReason_PinReset;
  } else if (reset_cause & NRF_POWER_RESETREAS_DOG_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Watchdog");
    reset_reason = kMfltRebootReason_HardwareWatchdog;
  } else if (reset_cause & NRF_POWER_RESETREAS_SREQ_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Software");
    reset_reason = kMfltRebootReason_SoftwareReset;
  } else if (reset_cause & NRF_POWER_RESETREAS_LOCKUP_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Lockup");
    reset_reason = kMfltRebootReason_Lockup;
#if defined (POWER_RESETREAS_LPCOMP_Msk)
  } else if (reset_cause & NRF_POWER_RESETREAS_LPCOMP_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" LPCOMP Wakeup");
    reset_reason = kMfltRebootReason_DeepSleep;
#endif
  } else if (reset_cause & NRF_POWER_RESETREAS_DIF_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Debug Interface Wakeup");
    reset_reason = kMfltRebootReason_DeepSleep;
#if defined (NRF_POWER_RESETREAS_VBUS_MASK)
  } else if (reset_cause & NRF_POWER_RESETREAS_VBUS_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" VBUS Wakeup");
    reset_reason = kMfltRebootReason_DeepSleep;
#endif
  } else if (reset_cause == 0) {
    // absence of a value, means a power on reset took place
    MEMFAULT_PRINT_RESET_INFO(" Power on Reset");
    reset_reason = kMfltRebootReason_PowerOnReset;
  } else {
    MEMFAULT_PRINT_RESET_INFO(" Unknown");
    reset_reason = kMfltRebootReason_Unknown;
  }
  return reset_reason;
}
#else
static eMemfaultRebootReason prv_decode_reset_resetreas(uint32_t reset_cause) {
  eMemfaultRebootReason reset_reason;
  if (reset_cause & NRF_RESET_RESETREAS_RESETPIN_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Pin Reset");
    reset_reason = kMfltRebootReason_PinReset;
  } else if (reset_cause & NRF_RESET_RESETREAS_DOG0_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Watchdog 0");
    reset_reason = kMfltRebootReason_HardwareWatchdog;
  } else if (reset_cause & NRF_RESET_RESETREAS_CTRLAP_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Debugger");
    reset_reason = kMfltRebootReason_SoftwareReset;
  } else if (reset_cause & NRF_RESET_RESETREAS_SREQ_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Software");
    reset_reason = kMfltRebootReason_SoftwareReset;
  } else if (reset_cause & NRF_RESET_RESETREAS_LOCKUP_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Lockup");
    reset_reason = kMfltRebootReason_Lockup;
  } else if (reset_cause & NRF_RESET_RESETREAS_OFF_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" GPIO Wakeup");
    reset_reason = kMfltRebootReason_DeepSleep;
  } else if (reset_cause & NRF_RESET_RESETREAS_LPCOMP_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" LPCOMP Wakeup");
    reset_reason = kMfltRebootReason_DeepSleep;
  } else if (reset_cause & NRF_RESET_RESETREAS_DIF_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Debug Interface Wakeup");
    reset_reason = kMfltRebootReason_DeepSleep;
#if NRF_RESET_HAS_NETWORK
  } else if (reset_cause & NRF_RESET_RESETREAS_LSREQ_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Software (Network)");
    reset_reason = kMfltRebootReason_SoftwareReset;
  } else if (reset_cause & NRF_RESET_RESETREAS_LLOCKUP_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Lockup (Network)");
    reset_reason = kMfltRebootReason_Lockup;
  } else if (reset_cause & NRF_RESET_RESETREAS_LDOG_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Watchdog (Network)");
    reset_reason = kMfltRebootReason_HardwareWatchdog;
  } else if (reset_cause & NRF_RESET_RESETREAS_MFORCEOFF_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Force off (Network)");
    reset_reason = kMfltRebootReason_SoftwareReset;
  } else if (reset_cause & NRF_RESET_RESETREAS_LCTRLAP_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Debugger (Network)");
    reset_reason = kMfltRebootReason_SoftwareReset;
#endif
  } else if (reset_cause & NRF_RESET_RESETREAS_VBUS_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" VBUS Wakeup");
    reset_reason = kMfltRebootReason_DeepSleep;
  } else if (reset_cause & NRF_RESET_RESETREAS_DOG1_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Watchdog 1");
    reset_reason = kMfltRebootReason_HardwareWatchdog;
  } else if (reset_cause & NRF_RESET_RESETREAS_NFC_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" NFC Wakeup");
    reset_reason = kMfltRebootReason_DeepSleep;
  } else if (reset_cause == 0) {
    // absence of a value, means a power on reset took place
    MEMFAULT_PRINT_RESET_INFO(" Power on Reset");
    reset_reason = kMfltRebootReason_PowerOnReset;
  } else {
    MEMFAULT_PRINT_RESET_INFO(" Unknown");
    reset_reason = kMfltRebootReason_Unknown;
  }

  return reset_reason;
}
#endif

MEMFAULT_WEAK
void memfault_reboot_reason_get(sResetBootupInfo *info) {
  MEMFAULT_SDK_ASSERT(info != NULL);

#if NRF_POWER_HAS_RESETREAS
  volatile uint32_t *resetreas_reg = &NRF_POWER->RESETREAS;
#else
  volatile uint32_t *resetreas_reg = &NRF_RESET->RESETREAS;
#endif

  const uint32_t reset_cause = *resetreas_reg;

  MEMFAULT_LOG_INFO("Reset Reason, RESETREAS=0x%" PRIx32, reset_cause);
  MEMFAULT_PRINT_RESET_INFO("Reset Causes: ");

  eMemfaultRebootReason reset_reason;
#if NRF_POWER_HAS_RESETREAS
  reset_reason = prv_decode_power_resetreas(reset_cause);
#else
  reset_reason = prv_decode_reset_resetreas(reset_cause);
#endif

#if CONFIG_MEMFAULT_CLEAR_RESET_REG
  *resetreas_reg |= reset_cause;
#endif

  *info = (sResetBootupInfo) {
    .reset_reason_reg = reset_cause,
    .reset_reason = reset_reason,
  };
}
