//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! A port of dependency functions for Memfault metrics subsystem using the ESP8266 SDK.
//!
//! @note For test purposes, the heartbeat interval can be changed to a faster period
//! by using the following CFLAG:
//!   -DMEMFAULT_METRICS_HEARTBEAT_INTERVAL_SECS=15

#include "sdkconfig.h"

#include "memfault/core/debug_log.h"
#include "memfault/core/reboot_tracking.h"
#include "memfault/metrics/metrics.h"
#include "memfault/metrics/platform/timer.h"

#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "FreeRTOS.h"
#include "task.h"

MEMFAULT_WEAK
void memfault_esp_metric_timer_dispatch(MemfaultPlatformTimerCallback handler) {
  if (handler == NULL) {
    return;
  }

#if CONFIG_MEMFAULT_HEARTBEAT_TRACK_HEAP_USAGE
  // We are about to service heartbeat data so get the latest stats for the
  // statistics being automatically tracked by the port
  memfault_metrics_heartbeat_set_unsigned(MEMFAULT_METRICS_KEY(Heap_FreeSize),
                                          heap_caps_get_free_size(0));
  memfault_metrics_heartbeat_set_unsigned(MEMFAULT_METRICS_KEY(Heap_MinFreeSize),
                                          heap_caps_get_minimum_free_size(0));
#endif

#if CONFIG_MEMFAULT_HEARTBEAT_TRACK_MAIN_STACK_HWM

#if INCLUDE_xTaskGetHandle == 0
 #error "Add #define INCLUDE_xTaskGetHandle 1 to FreeRTOSConfig.h"
#endif
  TaskHandle_t xHandle = xTaskGetHandle("uiT");

  memfault_metrics_heartbeat_set_unsigned(
      MEMFAULT_METRICS_KEY(MainTask_StackHighWaterMarkWords),
      uxTaskGetStackHighWaterMark(xHandle));
#endif

  handler();
}

static void prv_metric_timer_handler(void *arg) {
  memfault_reboot_tracking_reset_crash_count();

  // NOTE: This timer runs once per MEMFAULT_METRICS_HEARTBEAT_INTERVAL_SECS where the default is
  // once per hour.
  MemfaultPlatformTimerCallback *metric_timer_handler = (MemfaultPlatformTimerCallback *)arg;
  memfault_esp_metric_timer_dispatch(metric_timer_handler);
}

bool memfault_platform_metrics_timer_boot(uint32_t period_sec,
                                          MemfaultPlatformTimerCallback callback) {
  const esp_timer_create_args_t periodic_timer_args = {
    .callback = &prv_metric_timer_handler,
    .arg = callback,
    .name = "mflt"
  };

  esp_timer_handle_t periodic_timer;
  ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));

  const int64_t us_per_sec = 1000000;
  ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, period_sec * us_per_sec));

  return true;
}
