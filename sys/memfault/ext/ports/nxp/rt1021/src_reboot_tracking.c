//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! A port for recovering reset reason information by reading the "System Reset
//! Controller" (SRC)'s "Reset Status Register" (SRC_SRSR).
//!
//! More details can be found in the chapter "21.8.3 SRC Reset Status Register
//! (SRC_SRSR)" in the RT1021 Reference Manual

#include <inttypes.h>

#include "MIMXRT1021.h"
#include "memfault/components.h"
#include "memfault/ports/reboot_reason.h"

#if MEMFAULT_ENABLE_REBOOT_DIAG_DUMP
#define MEMFAULT_PRINT_RESET_INFO(...) MEMFAULT_LOG_INFO(__VA_ARGS__)
#else
#define MEMFAULT_PRINT_RESET_INFO(...)
#endif

void memfault_reboot_reason_get(sResetBootupInfo *info) {
  const uint32_t reset_cause = SRC->SRSR & (SRC_SRSR_W1C_BITS_MASK | SRC_SRSR_TEMPSENSE_RST_B_MASK);
  eMemfaultRebootReason reset_reason = kMfltRebootReason_Unknown;

  // clear the SRC Reset Status Register by writing a 1 to each bit
#if MEMFAULT_REBOOT_REASON_CLEAR
  SRC->SRSR = SRC_SRSR_W1C_BITS_MASK;
#endif

  MEMFAULT_LOG_INFO("Reset Reason, SRC_SRSR=0x%" PRIx32, reset_cause);

  MEMFAULT_PRINT_RESET_INFO("Reset Cause: ");
  if (reset_cause & SRC_SRSR_JTAG_SW_RST_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Software");
    reset_reason = kMfltRebootReason_SoftwareReset;
  } else if (reset_cause & SRC_SRSR_TEMPSENSE_RST_B_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Temp Sensor");
    reset_reason = kMfltRebootReason_UnknownError;
  } else if (reset_cause & SRC_SRSR_WDOG3_RST_B_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" HW Watchdog3");
    reset_reason = kMfltRebootReason_HardwareWatchdog;
  } else if (reset_cause & SRC_SRSR_WDOG_RST_B_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" HW Watchdog");
    // reset_cause can be use to disambiguate the watchdog types
    reset_reason = kMfltRebootReason_HardwareWatchdog;
  } else if (reset_cause & SRC_SRSR_JTAG_RST_B_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Debugger");
    reset_reason = kMfltRebootReason_DebuggerHalted;
  } else if (reset_cause & SRC_SRSR_IPP_USER_RESET_B_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Button");
    reset_reason = kMfltRebootReason_ButtonReset;
  } else if (reset_cause & SRC_SRSR_CSU_RESET_B_MASK) {
    // Central Security Unit triggered the reset (see Security Reference
    // Manual)
    MEMFAULT_PRINT_RESET_INFO(" CSU");
    reset_reason = kMfltRebootReason_UnknownError;
  } else if (reset_cause & SRC_SRSR_LOCKUP_SYSRESETREQ_MASK) {
    MEMFAULT_PRINT_RESET_INFO(" Lockup");
    reset_reason = kMfltRebootReason_Lockup;
  } else if (reset_cause & SRC_SRSR_IPP_RESET_B_MASK) {
    // TODO this might be equivalent to POR...
    MEMFAULT_PRINT_RESET_INFO(" Pin Reset");
    reset_reason = kMfltRebootReason_PinReset;
  } else {
    MEMFAULT_PRINT_RESET_INFO(" Unknown");
  }

  *info = (sResetBootupInfo){
    .reset_reason_reg = reset_cause,
    .reset_reason = reset_reason,
  };
}
