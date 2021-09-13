//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! An example implementation of the logging memfault API for the ESP32 platform

#include "memfault/core/platform/debug_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "memfault/config.h"

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

void memfault_platform_hexdump(eMemfaultPlatformLogLevel level, const void *data, size_t data_len) {
  switch (level) {
    case kMemfaultPlatformLogLevel_Debug:
      ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, data_len, ESP_LOG_DEBUG);
      break;

    case kMemfaultPlatformLogLevel_Info:
      ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, data_len, ESP_LOG_INFO);
      break;

    case kMemfaultPlatformLogLevel_Warning:
      ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, data_len, ESP_LOG_WARN);
      break;

    case kMemfaultPlatformLogLevel_Error:
      ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, data_len, ESP_LOG_ERROR);
      break;
    default:
      break;
  }
}
