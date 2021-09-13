#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Different types describing information collected as part of "Trace Events"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MfltResetReason {
  kMfltRebootReason_Unknown = 0x0000,

  //
  // Normal Resets
  //

  kMfltRebootReason_UserShutdown = 0x0001,
  kMfltRebootReason_UserReset = 0x0002,
  kMfltRebootReason_FirmwareUpdate = 0x0003,
  kMfltRebootReason_LowPower = 0x0004,
  kMfltRebootReason_DebuggerHalted = 0x0005,
  kMfltRebootReason_ButtonReset = 0x0006,
  kMfltRebootReason_PowerOnReset = 0x0007,
  kMfltRebootReason_SoftwareReset = 0x0008,

  // MCU went through a full reboot due to exit from lowest power state
  kMfltRebootReason_DeepSleep = 0x0009,
  // MCU reset pin was toggled
  kMfltRebootReason_PinReset = 0x000A,

  //
  // Error Resets
  //

  // Can be used to flag an unexpected reset path. i.e NVIC_SystemReset()
  // being called without any reboot logic getting invoked.
  kMfltRebootReason_UnknownError = 0x8000,
  kMfltRebootReason_Assert = 0x8001,

  // Deprecated in favor of HardwareWatchdog & SoftwareWatchdog. This way,
  // the amount of watchdogs not caught by software can be easily tracked
  kMfltRebootReason_WatchdogDeprecated = 0x8002,

  kMfltRebootReason_BrownOutReset = 0x8003,
  kMfltRebootReason_Nmi = 0x8004, // Non-Maskable Interrupt

  // More details about nomenclature in https://mflt.io/root-cause-watchdogs
  kMfltRebootReason_HardwareWatchdog = 0x8005,
  kMfltRebootReason_SoftwareWatchdog = 0x8006,

  // A reset triggered due to the MCU losing a stable clock. This can happen,
  // for example, if power to the clock is cut or the lock for the PLL is lost.
  kMfltRebootReason_ClockFailure = 0x8007,

  // Resets from Arm Faults
  kMfltRebootReason_BusFault = 0x9100,
  kMfltRebootReason_MemFault = 0x9200,
  kMfltRebootReason_UsageFault = 0x9300,
  kMfltRebootReason_HardFault = 0x9400,
  // A reset which is triggered when the processor faults while already
  // executing from a fault handler.
  kMfltRebootReason_Lockup = 0x9401,
} eMemfaultRebootReason;

#ifdef __cplusplus
}
#endif
