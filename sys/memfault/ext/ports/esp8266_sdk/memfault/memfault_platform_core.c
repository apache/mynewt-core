//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Entry point for initialization of Memfault SDK.

#include "memfault/esp8266_port/core.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "memfault/config.h"
#include "memfault/core/build_info.h"
#include "memfault/core/compiler.h"
#include "memfault/core/data_packetizer.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/event_storage.h"
#include "memfault/core/platform/core.h"
#include "memfault/core/platform/debug_log.h"
#include "memfault/core/reboot_tracking.h"
#include "memfault/core/trace_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"

#include "esp_timer.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

#ifndef MEMFAULT_DEBUG_LOG_BUFFER_SIZE_BYTES
#  define MEMFAULT_DEBUG_LOG_BUFFER_SIZE_BYTES (128)
#endif

static const char *TAG __attribute__((unused)) = "mflt";

void memfault_platform_log(eMemfaultPlatformLogLevel level, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  char log_buf[MEMFAULT_DEBUG_LOG_BUFFER_SIZE_BYTES];
  vsnprintf(log_buf, sizeof(log_buf), fmt, args);

  switch (level) {
    case kMemfaultPlatformLogLevel_Debug:
      ESP_LOGD(TAG, "%s", log_buf);
      break;

    case kMemfaultPlatformLogLevel_Info:
      ESP_LOGI(TAG, "%s", log_buf);
      break;

    case kMemfaultPlatformLogLevel_Warning:
      ESP_LOGW(TAG, "%s", log_buf);
      break;

    case kMemfaultPlatformLogLevel_Error:
      ESP_LOGE(TAG, "%s", log_buf);
      break;
    default:
      break;
  }

  va_end(args);
}

void memfault_platform_log_raw(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  vprintf(fmt, args);
  printf("\n");

  va_end(args);
}

static SemaphoreHandle_t s_memfault_lock;

void memfault_lock(void) {
  xSemaphoreTakeRecursive(s_memfault_lock, portMAX_DELAY);
}

void memfault_unlock(void) {
  xSemaphoreGiveRecursive(s_memfault_lock);
}

uint64_t memfault_platform_get_time_since_boot_ms(void) {
  const int64_t time_since_boot_us = esp_timer_get_time();
  return  (uint64_t) (time_since_boot_us / 1000) /* us per ms */;
}

bool memfault_arch_is_inside_isr(void) {
  return xPortInIsrContext();
}

MEMFAULT_WEAK
void memfault_platform_halt_if_debugging(void) { }

MEMFAULT_WEAK
void memfault_esp_port_boot(void) {
  s_memfault_lock = xSemaphoreCreateRecursiveMutex();

  memfault_build_info_dump();
  memfault_esp_port_coredump_storage_boot();

#if CONFIG_MEMFAULT_EVENT_COLLECTION_ENABLED
  memfault_esp_port_event_collection_boot();
#endif

#if CONFIG_MEMFAULT_CLI_ENABLED
  memfault_register_cli();
#endif
}
