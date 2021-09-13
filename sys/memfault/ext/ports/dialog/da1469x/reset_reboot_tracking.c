//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

//! @file
//!
//! Map DA1469x reboot reasons to memfault definitions

#include "memfault/ports/reboot_reason.h"

#include "DA1469xAB.h"
#include "sdk_defs.h"

__RETAINED_UNINIT static uint8_t s_reboot_tracking[MEMFAULT_REBOOT_TRACKING_REGION_SIZE];

void memfault_platform_reboot_tracking_boot(void) {
  sResetBootupInfo reset_info = { 0 };
  memfault_reboot_reason_get(&reset_info);
  memfault_reboot_tracking_boot(s_reboot_tracking, &reset_info);
}

void memfault_reboot_reason_get(sResetBootupInfo *info) {
  eMemfaultRebootReason reset_reason;

  uint32_t reset_cause = CRG_TOP->RESET_STAT_REG;
  if (reset_cause && (CRG_TOP_RESET_STAT_REG_CMAC_WDOGRESET_STAT_Msk |
                      CRG_TOP_RESET_STAT_REG_SWD_HWRESET_STAT_Msk    |
                      CRG_TOP_RESET_STAT_REG_WDOGRESET_STAT_Msk      |
                      CRG_TOP_RESET_STAT_REG_SWRESET_STAT_Msk        |
                      CRG_TOP_RESET_STAT_REG_HWRESET_STAT_Msk        |
                      CRG_TOP_RESET_STAT_REG_PORESET_STAT_Msk)) {
    reset_reason = kMfltRebootReason_PowerOnReset;
  } else if (reset_cause && CRG_TOP_RESET_STAT_REG_CMAC_WDOGRESET_STAT_Msk) {
    reset_reason = kMfltRebootReason_SoftwareWatchdog;
  } else if (reset_cause && CRG_TOP_RESET_STAT_REG_SWD_HWRESET_STAT_Msk) {
    reset_reason = kMfltRebootReason_DebuggerHalted;
  } else if (reset_cause && CRG_TOP_RESET_STAT_REG_SWRESET_STAT_Msk) {
    reset_reason = kMfltRebootReason_SoftwareReset;
  } else if(reset_cause && CRG_TOP_RESET_STAT_REG_HWRESET_STAT_Msk) {
    reset_reason = kMfltRebootReason_ButtonReset;
  } else {
    reset_reason = kMfltRebootReason_Unknown;
  }

  *info = (sResetBootupInfo) {
    .reset_reason_reg = reset_cause,
    .reset_reason = reset_reason,
  };
}
