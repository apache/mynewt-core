//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! A port for recovering reset reason information by reading the
//! "Reset Control Module" (RCM)'s "System Reset Status" (SRS) Register.
//!
//! See "26.4.3 System Reset Status Register" of S32K-RM for more details

#include "memfault/ports/reboot_reason.h"

#include "memfault/config.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/reboot_reason_types.h"
#include "memfault/core/sdk_assert.h"

#include "device_registers.h"

#if MEMFAULT_ENABLE_REBOOT_DIAG_DUMP
#define MEMFAULT_PRINT_RESET_INFO(...) MEMFAULT_LOG_INFO(__VA_ARGS__)
#else
#define MEMFAULT_PRINT_RESET_INFO(...)
#endif

void memfault_reboot_reason_get(sResetBootupInfo *info) {
  MEMFAULT_SDK_ASSERT(info != NULL);

  // NB: The S32 has two reset registers:
  //   System Reset Status (SRS) which contains reset reasons for most recent boot
  //   Sticky System Reset Status (SSRS) which contains all reasons for resets since last POR
  //
  // We'll just use the non-sticky variant for the port!

  const uint32_t reset_cause = RCM->SRS;

  eMemfaultRebootReason reset_reason = kMfltRebootReason_Unknown;

  MEMFAULT_LOG_INFO("Reset Reason, SRS=0x%" PRIx32, reset_cause);
  MEMFAULT_PRINT_RESET_INFO("Reset Causes: ");

  // From the S32K-RM:
  //
  // The reset value of this register depends on the reset source:
  //   POR (including LVD) — 0x82
  //   LVD (without POR) — 0x02
  // Other reset — a bit is set if its corresponding reset source caused the reset

  if ((reset_cause & RCM_SRS_LVD(1)) && (reset_cause & RCM_SRS_POR(1))) {
    MEMFAULT_PRINT_RESET_INFO(" Low or High Voltage");
    reset_reason = kMfltRebootReason_BrownOutReset;
  } else if (reset_cause & RCM_SRS_POR(1)) {
    MEMFAULT_PRINT_RESET_INFO(" POR");
    reset_reason = kMfltRebootReason_PowerOnReset;
  } else if (reset_cause & RCM_SRS_MDM_AP(1)) {
    MEMFAULT_PRINT_RESET_INFO(" Debugger (AP)");
    reset_reason = kMfltRebootReason_SoftwareReset;
  } else if (reset_cause & RCM_SRS_SW(1)) {
    MEMFAULT_PRINT_RESET_INFO(" Software");
    reset_reason = kMfltRebootReason_SoftwareReset;
  } else if (reset_cause & RCM_SRS_JTAG(1)) {
    MEMFAULT_PRINT_RESET_INFO(" Debugger (JTAG)");
    reset_reason = kMfltRebootReason_SoftwareReset;
  } else if (reset_cause & RCM_SRS_PIN(1)) {
    MEMFAULT_PRINT_RESET_INFO(" Reset Pin");
    reset_reason = kMfltRebootReason_ButtonReset;
  } else if (reset_cause & RCM_SRS_LOCKUP(1)) {
    MEMFAULT_PRINT_RESET_INFO(" Lockup");
    reset_reason = kMfltRebootReason_Lockup;
  } else if (reset_cause & RCM_SRS_WDOG(1)) {
    MEMFAULT_PRINT_RESET_INFO(" Hardware Watchdog");
    reset_reason = kMfltRebootReason_HardwareWatchdog;
  } else if (reset_cause & RCM_SRS_LOL(1)) {
    MEMFAULT_PRINT_RESET_INFO(" Loss of Lock in PLL/FLL");
    reset_reason = kMfltRebootReason_ClockFailure;
  } else if (reset_cause & RCM_SRS_LOC(1)) {
    MEMFAULT_PRINT_RESET_INFO(" Loss of Clock");
    reset_reason = kMfltRebootReason_ClockFailure;
  }

  *info = (sResetBootupInfo) {
    .reset_reason_reg = reset_cause,
    .reset_reason = reset_reason,
  };
}
