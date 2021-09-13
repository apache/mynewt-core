//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#include "memfault/metrics/metrics.h"
#include "memfault/metrics/platform/timer.h"

#include "app_easy_timer.h"
#include "datasheet.h"
#include "lld_evt.h"

#include <stdbool.h>

bool memfault_platform_metrics_timer_boot(uint32_t period_sec,
                                          MemfaultPlatformTimerCallback callback) {
  bool rc = true;
  // Schedule a timer to invoke callback() repeatedly after period_sec
  if (app_easy_timer(MS_TO_TIMERUNITS(period_sec * 1000), callback) ==
          EASY_TIMER_INVALID_TIMER) {
    rc = false;
  }
  return rc;
}

uint64_t memfault_platform_get_time_since_boot_ms(void) {
  if (GetBits16(CLK_RADIO_REG, BLE_ENABLE) == 0) {
    // The BLE core timer is not running so we just return 0. Generally, the core should always be running
    return 0;
  }

  uint64_t time = (uint64_t)lld_evt_time_get();
  // LLD time is in units of 625us, convert to ms.
  time = (time * 625ULL) / 1000ULL;

  // return time since boot in ms, this is used for relative timings.
  return time;
}
