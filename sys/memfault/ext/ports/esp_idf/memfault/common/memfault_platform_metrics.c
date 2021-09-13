//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! A port of dependency functions for Memfault metrics subsystem using FreeRTOS.
//!
//! @note For test purposes, the heartbeat interval can be changed to a faster period
//! by using the following CFLAG:
//!   -DMEMFAULT_METRICS_HEARTBEAT_INTERVAL_SECS=15

#include "memfault/core/debug_log.h"
#include "memfault/core/reboot_tracking.h"
#include "memfault/esp_port/metrics.h"
#include "memfault/metrics/metrics.h"
#include "memfault/metrics/platform/timer.h"

#include "esp_timer.h"

MEMFAULT_WEAK
void memfault_esp_metric_timer_dispatch(MemfaultPlatformTimerCallback handler) {
  if (handler == NULL) {
    return;
  }
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
