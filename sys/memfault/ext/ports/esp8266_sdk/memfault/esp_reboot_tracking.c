//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Reads the reset reason information saved in the ESP8266 RTC backup domain.
//! I believe this info itself is copied by the bootloader from another register but
//! could not find any documentation about it.

#include "memfault/ports/reboot_reason.h"

#include "sdkconfig.h"
#include "esp_system.h"
#include "internal/esp_system_internal.h"
#include "esp8266/rtc_register.h"
#include "esp8266/rom_functions.h"
#include "esp_libc.h"

#include "memfault/core/debug_log.h"
#include "memfault/config.h"

#if MEMFAULT_ENABLE_REBOOT_DIAG_DUMP
#define MEMFAULT_PRINT_RESET_INFO(...) MEMFAULT_LOG_INFO(__VA_ARGS__)
#else
#define MEMFAULT_PRINT_RESET_INFO(...)
#endif

void memfault_reboot_reason_get(sResetBootupInfo *info) {
  eMemfaultRebootReason reboot_reason = kMfltRebootReason_Unknown;
  int esp_reset_cause = (int)esp_reset_reason();

  MEMFAULT_LOG_INFO("ESP Reset Cause 0x%" PRIx32, esp_reset_cause);
  MEMFAULT_PRINT_RESET_INFO("Reset Causes: ");

  switch (esp_reset_cause) {
    case ESP_RST_POWERON:
      reboot_reason = kMfltRebootReason_PowerOnReset;
      MEMFAULT_PRINT_RESET_INFO(" Power On Reset");
      break;
    case ESP_RST_SW:
      reboot_reason = kMfltRebootReason_SoftwareReset;
      MEMFAULT_PRINT_RESET_INFO(" Software Reset");
      break;
    case ESP_RST_INT_WDT:
      reboot_reason = kMfltRebootReason_HardwareWatchdog;
      MEMFAULT_PRINT_RESET_INFO(" INT Watchdog");
      break;
    case ESP_RST_TASK_WDT:
      reboot_reason = kMfltRebootReason_HardwareWatchdog;
      MEMFAULT_PRINT_RESET_INFO(" Task Watchdog");
      break;
    case ESP_RST_WDT:
      // Empirically, once set it seems this state is sticky across resets until a POR takes place
      // so we don't automatically flag it as a watchdog
      MEMFAULT_PRINT_RESET_INFO(" Hardware Watchdog");
      break;
    case ESP_RST_DEEPSLEEP:
      reboot_reason = kMfltRebootReason_DeepSleep;
      MEMFAULT_PRINT_RESET_INFO(" Deep Sleep");
      break;
    case ESP_RST_BROWNOUT:
      reboot_reason = kMfltRebootReason_BrownOutReset;
      MEMFAULT_PRINT_RESET_INFO(" Brown Out");
      break;
    case ESP_RST_PANIC:
      reboot_reason = kMfltRebootReason_HardFault;
      MEMFAULT_PRINT_RESET_INFO(" Software Panic");
      break;
    default:
      MEMFAULT_PRINT_RESET_INFO(" Unknown");
      reboot_reason = kMfltRebootReason_UnknownError;
      break;
  }

  *info = (sResetBootupInfo) {
    .reset_reason_reg = esp_reset_cause,
    .reset_reason = reboot_reason,
  };
}
