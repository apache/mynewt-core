//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! A port for recovering reset reason information by reading the
//! "Reset Management Unit" (RMU)'s "Reset Cause" (RSTCAUSE) Register.
//!
//! More details can be found in the "RMU_RSTCAUSE Register" of the reference manual
//! for the specific EFR or EFM chip family. It makes use of APIs that are part of
//! the Gecko SDK.

#include "memfault/ports/reboot_reason.h"

#include "em_rmu.h"
#include "em_emu.h"

#include "memfault/config.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/reboot_reason_types.h"
#include "memfault/core/sdk_assert.h"

#if MEMFAULT_ENABLE_REBOOT_DIAG_DUMP
#define MEMFAULT_PRINT_RESET_INFO(...) MEMFAULT_LOG_INFO(__VA_ARGS__)
#else
#define MEMFAULT_PRINT_RESET_INFO(...)
#endif

#if defined(_RMU_RSTCAUSE_MASK)
static eMemfaultRebootReason prv_get_and_print_reason(uint32_t reset_cause) {
  // Find the RMU_RSTCAUSE Register data sheet for the EFM/EFR part for more details
  // For example, in the EFM32PG12 it's in section "8.3.2 RMU_RSTCAUSE Register"
  //
  // Note that some reset types are shared across EFM/EFR MCU familes. For the ones
  // that are not, we wrap the reason with an ifdef.
  if (reset_cause & RMU_RSTCAUSE_PORST) {
    MEMFAULT_PRINT_RESET_INFO(" Power on Reset");
    return kMfltRebootReason_PowerOnReset;
#if defined(RMU_RSTCAUSE_AVDDBOD)
  } else if (reset_cause & RMU_RSTCAUSE_AVDDBOD) {
    MEMFAULT_PRINT_RESET_INFO(" AVDD Brown Out");
    return kMfltRebootReason_BrownOutReset;
#endif
#if defined(RMU_RSTCAUSE_DVDDBOD)
  } else if (reset_cause & RMU_RSTCAUSE_DVDDBOD) {
    MEMFAULT_PRINT_RESET_INFO(" DVDD Brown Out");
    return kMfltRebootReason_BrownOutReset;
#endif
#if defined(RMU_RSTCAUSE_DECBOD)
  } else if (reset_cause & RMU_RSTCAUSE_DECBOD) {
    MEMFAULT_PRINT_RESET_INFO(" DEC Brown Out");
    return kMfltRebootReason_BrownOutReset;
#endif
  } else if (reset_cause & RMU_RSTCAUSE_LOCKUPRST) {
    MEMFAULT_PRINT_RESET_INFO(" Lockup");
    return kMfltRebootReason_Lockup;
  } else if (reset_cause & RMU_RSTCAUSE_SYSREQRST) {
    MEMFAULT_PRINT_RESET_INFO(" Software");
    return kMfltRebootReason_SoftwareReset;
  } else if (reset_cause & RMU_RSTCAUSE_WDOGRST) {
    MEMFAULT_PRINT_RESET_INFO(" Watchdog");
    return kMfltRebootReason_HardwareWatchdog;
#if defined(RMU_RSTCAUSE_EM4WURST)
  } else if (reset_cause & RMU_RSTCAUSE_EM4RST) {
    MEMFAULT_PRINT_RESET_INFO(" EM4 Wakeup");
    return kMfltRebootReason_DeepSleep;
#endif
  } else if (reset_cause & RMU_RSTCAUSE_EXTRST) {
    MEMFAULT_PRINT_RESET_INFO(" Pin Reset");
    return kMfltRebootReason_ButtonReset;
  }

  MEMFAULT_PRINT_RESET_INFO(" Unknown");
  return kMfltRebootReason_Unknown;
}
#elif defined(_EMU_RSTCTRL_MASK)

static eMemfaultRebootReason prv_get_and_print_reason(uint32_t reset_cause) {
  // Find the EMU_RSTCAUSE Register data sheet for the EFM/EFR part for more details
  // For example, in the EFR32xG21 it's in section "12.5.13 EMU_RSTCAUSE - Reset cause"
  //
  // Note that some reset types are shared across EFM/EFR MCU familes. For the ones
  // that are not, we wrap the reason with an ifdef.

  if (reset_cause & EMU_RSTCAUSE_POR) {
    MEMFAULT_PRINT_RESET_INFO(" Power on Reset");
    return kMfltRebootReason_PowerOnReset;
  } else if (reset_cause & EMU_RSTCAUSE_AVDDBOD) {
    MEMFAULT_PRINT_RESET_INFO(" AVDD Brown Out");
    return kMfltRebootReason_BrownOutReset;
  } else if (reset_cause & EMU_RSTCAUSE_IOVDD0BOD) {
    MEMFAULT_PRINT_RESET_INFO(" IOVDD0 Brown Out");
    return kMfltRebootReason_BrownOutReset;
  } else if (reset_cause & EMU_RSTCAUSE_DVDDBOD) {
    MEMFAULT_PRINT_RESET_INFO(" DVDD Brown Out");
    return kMfltRebootReason_BrownOutReset;
  } else if (reset_cause &  EMU_RSTCAUSE_DVDDLEBOD) {
    MEMFAULT_PRINT_RESET_INFO(" DVDDLE Brown Out");
    return kMfltRebootReason_BrownOutReset;
  } else if (reset_cause & EMU_RSTCAUSE_DECBOD) {
    MEMFAULT_PRINT_RESET_INFO(" DEC Brown Out");
    return kMfltRebootReason_BrownOutReset;
  } else if (reset_cause & EMU_RSTCAUSE_LOCKUP) {
    MEMFAULT_PRINT_RESET_INFO(" Lockup");
    return kMfltRebootReason_Lockup;
  } else if (reset_cause & EMU_RSTCAUSE_SYSREQ) {
    MEMFAULT_PRINT_RESET_INFO(" Software");
    return kMfltRebootReason_SoftwareReset;
  } else if (reset_cause & EMU_RSTCAUSE_WDOG0) {
    MEMFAULT_PRINT_RESET_INFO(" Watchdog 0");
    return kMfltRebootReason_HardwareWatchdog;
  } else if (reset_cause & EMU_RSTCAUSE_WDOG1) {
    MEMFAULT_PRINT_RESET_INFO(" Watchdog 1");
    return kMfltRebootReason_HardwareWatchdog;
  } else if (reset_cause & EMU_RSTCAUSE_EM4) {
    MEMFAULT_PRINT_RESET_INFO(" EM4 Wakeup");
    return kMfltRebootReason_DeepSleep;
  } else if (reset_cause & EMU_RSTCAUSE_SETAMPER) {
    MEMFAULT_PRINT_RESET_INFO(" SE Tamper");
    return kMfltRebootReason_UnknownError;
  } else if (reset_cause & EMU_RSTCAUSE_SESYSREQ) {
    MEMFAULT_PRINT_RESET_INFO(" SE Software Reset");
    return kMfltRebootReason_SoftwareReset;
  } else if (reset_cause & EMU_RSTCAUSE_SELOCKUP) {
    MEMFAULT_PRINT_RESET_INFO(" SE Lockup");
    return kMfltRebootReason_Lockup;
  } else if (reset_cause & EMU_RSTCAUSE_PIN) {
    MEMFAULT_PRINT_RESET_INFO(" SE Pin Reset");
    return kMfltRebootReason_PinReset;
  }

  MEMFAULT_PRINT_RESET_INFO(" Unknown");
  return kMfltRebootReason_Unknown;
}
#else
#error "Unexpected RSTCTRL configuration"
#endif

void memfault_reboot_reason_get(sResetBootupInfo *info) {
  MEMFAULT_SDK_ASSERT(info != NULL);

  // This routine simply reads RMU->RSTCAUSE and zero's out
  // bits that aren't relevant to the reset. For more details
  // check out the logic in ${PATH_TO_GECKO_SDK}/platform/emlib/src/em_rmu.c
  const uint32_t reset_cause = RMU_ResetCauseGet();

  MEMFAULT_PRINT_RESET_INFO("Reset Reason, RSTCAUSE=0x%" PRIx32, reset_cause);
  MEMFAULT_PRINT_RESET_INFO("Reset Causes: ");

  eMemfaultRebootReason reset_reason = prv_get_and_print_reason(reset_cause);

#if MEMFAULT_REBOOT_REASON_CLEAR
  // we have read the reset information so clear the bits (since they are sticky across reboots)
  RMU_ResetCauseClear();
#endif

  *info = (sResetBootupInfo) {
    .reset_reason_reg = reset_cause,
    .reset_reason = reset_reason,
  };
}
