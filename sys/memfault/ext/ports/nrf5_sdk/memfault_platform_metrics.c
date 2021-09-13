//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//! A port of the memfault timer dependency functions which utilizes the nRF5 SDK app_timer module
//!
//! Options:
//!  MEMFAULT_PLATFORM_BOOT_TIMER_CUSTOM (default = 0)
//!   When set to 1, user must define memfault_platform_get_time_since_boot_ms.

#include "memfault/core/debug_log.h"
#include "memfault/core/platform/core.h"
#include "memfault/metrics/platform/timer.h"

#include "app_timer.h"
#include "nrf_log.h"

#if !MEMFAULT_PLATFORM_BOOT_TIMER_CUSTOM
#define MEMFAULT_PLATFORM_BOOT_TIMER_CUSTOM 0
#endif

APP_TIMER_DEF(m_mflt_metric_log_timer);
static MemfaultPlatformTimerCallback *s_registered_cb = NULL;
static uint32_t s_minutes_elapsed;
static uint32_t s_interval_minutes;

#if !MEMFAULT_PLATFORM_BOOT_TIMER_CUSTOM
typedef struct {
  uint32_t last_tick_count;
  // Worst case on the nrf52 we will have 32768 ticks per second
  //
  // 2^64 / (32768 * 86400 * 365 * 1000) = 17851 years ... so
  // not practical tick count overflow concern
  uint64_t time_since_boot_ticks;
} sMfltPlatformUptimeCtx;

static sMfltPlatformUptimeCtx s_uptime_ctx;

uint64_t memfault_platform_get_time_since_boot_ms(void) {
  uint32_t ticks_current = app_timer_cnt_get();
  uint32_t ticks_diff = app_timer_cnt_diff_compute(ticks_current, s_uptime_ctx.last_tick_count);

  const uint64_t curr_tick_count = s_uptime_ctx.time_since_boot_ticks + ticks_diff;
  const uint32_t ticks_per_sec = APP_TIMER_CLOCK_FREQ / (APP_TIMER_CONFIG_RTC_FREQUENCY + 1);
  const uint64_t curr_tick_time_ms = (1000 * curr_tick_count) / ticks_per_sec;
  return curr_tick_time_ms;
}

static void prv_update_boot_time(void) {
  const uint32_t ticks_current = app_timer_cnt_get();
  const uint32_t ticks_diff = app_timer_cnt_diff_compute(ticks_current, s_uptime_ctx.last_tick_count);

  s_uptime_ctx.time_since_boot_ticks += ticks_diff;
  s_uptime_ctx.last_tick_count = ticks_current;
}

#else
static void prv_update_boot_time(void) { }
#endif /* MEMFAULT_PLATFORM_BOOT_TIMER_CUSTOM */

static void prv_mflt_metric_timer(void *p_context) {
  prv_update_boot_time();

  // The customer can configure the PRESCALAR off them but by default it's a 24 bit counter running at 32kHz so it
  // overflows every 511 seconds. When a period is passed that is larger than this, use a 1 minute base timer and
  // fire on the nearest requested minute:
  if (s_interval_minutes != 0) {
    ++s_minutes_elapsed;
    if ((s_minutes_elapsed % s_interval_minutes) != 0) {
      return;
    }
  }
  if (s_registered_cb != NULL) {
    s_registered_cb();
  }
}

bool memfault_platform_metrics_timer_boot(uint32_t period_sec, MemfaultPlatformTimerCallback callback) {
  if (s_registered_cb != NULL) {
    MEMFAULT_LOG_ERROR("%s should only be called once", __func__);
    return false;
  }

  uint32_t err_code = app_timer_create(
      &m_mflt_metric_log_timer, APP_TIMER_MODE_REPEATED, prv_mflt_metric_timer);
  APP_ERROR_CHECK(err_code);

  s_registered_cb = callback;

  uint32_t period_ticks = APP_TIMER_TICKS(period_sec * 1000);
  if (period_ticks > APP_TIMER_MAX_CNT_VAL) {
    // Assume period_s is a multiple of 60, otherwise round to the nearest minute:
    s_interval_minutes = ROUNDED_DIV(period_sec, 60);
    s_minutes_elapsed = 0;
    period_ticks = APP_TIMER_TICKS(60 * 1000);
  } else {
    s_interval_minutes = 0;
  }
  err_code = app_timer_start(m_mflt_metric_log_timer, period_ticks, NULL);
  APP_ERROR_CHECK(err_code);
  return true;
}
