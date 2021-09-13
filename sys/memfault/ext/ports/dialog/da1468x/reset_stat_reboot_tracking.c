//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! A port for recovering reset reason information by reading the DA1468x's
//! "Reset Reason" RESET_STAT_REG Register.
//!
//! More details can be found in the "CRG Register File" (RESET_STAT_REG)"
//! section of the datasheet document for your specific chip.
#include "hw_cpm.h"
#include "osal.h"
#include "sdk_defs.h"

#include "memfault/config.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/reboot_reason_types.h"
#include "memfault/core/sdk_assert.h"
#include "memfault/ports/reboot_reason.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#if MEMFAULT_ENABLE_REBOOT_DIAG_DUMP
#define MEMFAULT_PRINT_RESET_INFO(...) MEMFAULT_LOG_INFO(__VA_ARGS__)
#else
#define MEMFAULT_PRINT_RESET_INFO(...)
#endif

// Use Dialog's way of locating variables to a no-init section. This allocation is very
// Dialog specific so we can put it here within the chip implementation.
__RETAINED_UNINIT MEMFAULT_ALIGNED(8)
static uint8_t s_reboot_tracking[MEMFAULT_REBOOT_TRACKING_REGION_SIZE];

// Called by the user application to get our reboot allocation registered with the core.
void memfault_platform_reboot_tracking_boot(void) {
  // For a detailed explanation about reboot reason storage options check out the guide at:
  //    https://mflt.io/reboot-reason-storage
  sResetBootupInfo reset_reason = {0};
  memfault_reboot_reason_get(&reset_reason);
  memfault_reboot_tracking_boot(s_reboot_tracking, &reset_reason);
}

// Private helper functions deal with the details of manipulating the CPU's
// reset reason register. On some CPUs this is more involved.
static uint32_t prv_reset_reason_get(void) {
  return CRG_TOP->RESET_STAT_REG;
}

static void prv_reset_reason_clear(uint32_t reset_reas_clear_mask) {
  (void) reset_reas_clear_mask;

  // Write zero to clear, not ones.
  CRG_TOP->RESET_STAT_REG = 0;
}

// Map chip-specific reset reasons to Memfault reboot reasons. Below is from the
// DA14683 datasheet.
//
// Bit Mode Symbol           Description Reset
// --- ---- ---------------- -----------------------------------------------------
// 0   R/W  PORESET_STAT     Indicates that a PowerOn Reset has happened.
// 1   R/W  HWRESET_STAT     Indicates that a HW Reset has happened.
// 2   R/W  SWRESET_STAT     Indicates that a SW Reset has happened.
// 3   R/W  WDOGRESET_STAT   Indicates that a Watchdog has happened.
//                           * Note that it is also set when a POReset has happened.
// 4   R/W  SWD_HWRESET_STAT Indicates that a write to SWD_RESET_REG has happened.
//                           * Note that it is also set when a POReset has happened.
void memfault_reboot_reason_get(sResetBootupInfo *info) {
  MEMFAULT_SDK_ASSERT(info != NULL);

  // Consume the reset reason register leaving it cleared in HW.
  // RESETREAS is part of the always on power domain so it's sticky until a full reset occurs
  // Therefore, we clear the bits which were set so that don't get logged in future reboots as well
  const uint32_t reset_reason_reg = prv_reset_reason_get();
  prv_reset_reason_clear(reset_reason_reg);

  // Assume "no bits set" implies POR.
  uint32_t reset_reason = kMfltRebootReason_PowerOnReset;

  // Note that POR also sets WD, and SWD bits so check order is important.
  if (reset_reason_reg & CRG_TOP_RESET_STAT_REG_PORESET_STAT_Msk) {
    // Important to check PORESET_STAT first.
    MEMFAULT_PRINT_RESET_INFO(" Power on Reset");
    reset_reason = kMfltRebootReason_PowerOnReset;
  } else if (reset_reason_reg & CRG_TOP_RESET_STAT_REG_SWD_HWRESET_STAT_Msk) {
    // True SWD reset since POR flag was not set. We just map it to SW reset.
    MEMFAULT_PRINT_RESET_INFO(" Debugger (SWD)");
    reset_reason = kMfltRebootReason_SoftwareReset;
  } else if (reset_reason_reg & CRG_TOP_RESET_STAT_REG_WDOGRESET_STAT_Msk) {
    // True WD reset since POR flag was not set.
    MEMFAULT_PRINT_RESET_INFO(" Watchdog");
    reset_reason = kMfltRebootReason_HardwareWatchdog;
  } else if (reset_reason_reg & CRG_TOP_RESET_STAT_REG_SWRESET_STAT_Msk) {
    MEMFAULT_PRINT_RESET_INFO(" Software");
    reset_reason = kMfltRebootReason_SoftwareReset;
  } else if (reset_reason_reg & CRG_TOP_RESET_STAT_REG_HWRESET_STAT_Msk) {
    MEMFAULT_PRINT_RESET_INFO(" Pin Reset");
    reset_reason = kMfltRebootReason_PinReset;
  }

  *info = (sResetBootupInfo) {
    .reset_reason_reg = reset_reason_reg,
    .reset_reason     = reset_reason,
  };
}
