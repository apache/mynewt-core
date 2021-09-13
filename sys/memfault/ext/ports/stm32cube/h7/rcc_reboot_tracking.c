//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! A port for recovering reset reason information by reading the
//! "Reset and Clock Control" (RCC)'s "Reset Status Register" (RSR).
//!
//! More details can be found in the "RCC Reset Status Register (RCC_RSR)"
//! section of the STM32H7 family reference manual.

#include "memfault/ports/reboot_reason.h"

#include "stm32h7xx_ll_rcc.h"

#include "memfault/config.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/reboot_reason_types.h"
#include "memfault/core/sdk_assert.h"

#if MEMFAULT_ENABLE_REBOOT_DIAG_DUMP
#define MEMFAULT_PRINT_RESET_INFO(...) MEMFAULT_LOG_INFO(__VA_ARGS__)
#else
#define MEMFAULT_PRINT_RESET_INFO(...)
#endif

//! Mappings come from "8.4.4 Reset source identification" of
//! "STM32H742, STM32H743/753 and STM32H750" Reference Manual
typedef enum ResetSource {
  kResetSource_PwrPor = (RCC_RSR_PORRSTF | RCC_RSR_PINRSTF | RCC_RSR_BORRSTF |
                         RCC_RSR_D2RSTF | RCC_RSR_D1RSTF | RCC_RSR_CPURSTF),
  kResetSource_Pin = (RCC_RSR_PINRSTF | RCC_RSR_CPURSTF),
  kResetSource_PwrBor = (RCC_RSR_PINRSTF | RCC_RSR_BORRSTF | RCC_RSR_CPURSTF),
  kResetSource_Software = (RCC_RSR_SFTRSTF | RCC_RSR_PINRSTF | RCC_RSR_CPURSTF),
  kResetSource_Cpu = RCC_RSR_CPURSTF,
  kResetSource_Wwdg = (RCC_RSR_WWDG1RSTF | RCC_RSR_PINRSTF | RCC_RSR_CPURSTF),
  kResetSource_Iwdg = (RCC_RSR_IWDG1RSTF | RCC_RSR_PINRSTF | RCC_RSR_CPURSTF),
  kResetSource_D1Exit = RCC_RSR_D1RSTF,
  kResetSource_D2Exit = RCC_RSR_D2RSTF,
  kResetSource_LowPowerError =
      (RCC_RSR_LPWRRSTF | RCC_RSR_PINRSTF | RCC_RSR_CPURSTF),
} eResetSource;

void memfault_reboot_reason_get(sResetBootupInfo *info) {
  const uint32_t reset_cause = RCC->RSR;

  eMemfaultRebootReason reset_reason = kMfltRebootReason_Unknown;

  MEMFAULT_PRINT_RESET_INFO("Reset Reason, RCC_RSR=0x%" PRIx32, reset_cause);
  MEMFAULT_PRINT_RESET_INFO("Reset Causes: ");

  const uint32_t reset_mask_all =
      (RCC_RSR_IWDG1RSTF | RCC_RSR_CPURSTF | RCC_RSR_D1RSTF | RCC_RSR_D2RSTF |
       RCC_RSR_BORRSTF | RCC_RSR_PINRSTF | RCC_RSR_PORRSTF | RCC_RSR_SFTRSTF |
       RCC_RSR_WWDG1RSTF | RCC_RSR_LPWRRSTF);

  switch (reset_cause & reset_mask_all) {
    case kResetSource_PwrPor:
      MEMFAULT_PRINT_RESET_INFO(" Power on Reset");
      reset_reason = kMfltRebootReason_PowerOnReset;
      break;
    case kResetSource_Pin:
      MEMFAULT_PRINT_RESET_INFO(" Pin Reset");
      reset_reason = kMfltRebootReason_PinReset;
      break;
    case kResetSource_PwrBor:
      MEMFAULT_PRINT_RESET_INFO(" Brown out");
      reset_reason = kMfltRebootReason_BrownOutReset;
      break;
    case kResetSource_Software:
      MEMFAULT_PRINT_RESET_INFO(" Software");
      reset_reason = kMfltRebootReason_SoftwareReset;
      break;
    case kResetSource_Cpu:
      // A reset generated via RCC_AHB3RSTR
      MEMFAULT_PRINT_RESET_INFO(" Cpu");
      reset_reason = kMfltRebootReason_SoftwareReset;
      break;
    case kResetSource_Wwdg:
      MEMFAULT_PRINT_RESET_INFO(" Window Watchdog");
      reset_reason = kMfltRebootReason_HardwareWatchdog;
      break;
    case kResetSource_Iwdg:
      MEMFAULT_PRINT_RESET_INFO(" Independent Watchdog");
      reset_reason = kMfltRebootReason_HardwareWatchdog;
      break;
    case kResetSource_D1Exit:
      MEMFAULT_PRINT_RESET_INFO(" D1 Low Power Exit");
      reset_reason = kMfltRebootReason_LowPower;
      break;
    case kResetSource_D2Exit:
      MEMFAULT_PRINT_RESET_INFO(" D2 Low Power Exit");
      reset_reason = kMfltRebootReason_LowPower;
      break;
    case kResetSource_LowPowerError:
      MEMFAULT_PRINT_RESET_INFO(" Illegal D1 DStandby / CStop");
      reset_reason = kMfltRebootReason_UnknownError;
      break;
    default:
      MEMFAULT_PRINT_RESET_INFO(" Unknown");
      break;
  }

#if MEMFAULT_REBOOT_REASON_CLEAR
  // we have read the reset information so clear the bits (since they are sticky across reboots)
  __HAL_RCC_CLEAR_RESET_FLAGS();
#endif

  *info = (sResetBootupInfo) {
    .reset_reason_reg = reset_cause,
    .reset_reason = reset_reason,
  };
}
