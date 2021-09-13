//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! A port for recovering reset reason information by reading the
//! Reset Controller's (RSTC) "Reset Cause" (RCAUSE) register.
//!
//! More details can be found in the "RSTC – Reset Controller" of the
//! "SAM L10/L11 Family" Datasheet

#include "memfault/ports/reboot_reason.h"

#include "sam.h"

void memfault_reboot_reason_get(sResetBootupInfo *info) {
  const uint32_t reset_cause = (uint32_t)REG_RSTC_RCAUSE;
  eMemfaultRebootReason reset_reason = kMfltRebootReason_Unknown;

  if (reset_cause & RSTC_RCAUSE_POR_Msk) {
    reset_reason = kMfltRebootReason_PowerOnReset;
  } else if (reset_cause & RSTC_RCAUSE_BODCORE_Msk) {
    reset_reason = kMfltRebootReason_BrownOutReset;
  } else if (reset_cause & RSTC_RCAUSE_BODVDD_Msk) {
    reset_reason = kMfltRebootReason_BrownOutReset;
  } else if (reset_cause & RSTC_RCAUSE_EXT_Msk) {
    reset_reason = kMfltRebootReason_PinReset;
  } else if (reset_cause & RSTC_RCAUSE_WDT_Msk) {
    reset_reason = kMfltRebootReason_HardwareWatchdog;
  } else if (reset_cause & RSTC_RCAUSE_SYST_Msk) {
    reset_reason = kMfltRebootReason_SoftwareReset;
  }

  *info = (sResetBootupInfo) {
    .reset_reason_reg = reset_cause,
    .reset_reason = reset_reason,
  };
}
