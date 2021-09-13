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

#include "memfault/ports/freertos.h"

#include "memfault/core/compiler.h"
#include "memfault/metrics/metrics.h"
#include "memfault/metrics/platform/timer.h"

#include "FreeRTOS.h"
#include "timers.h"

static MemfaultPlatformTimerCallback *s_metric_timer_cb = NULL;
static void prv_metric_timer_callback(MEMFAULT_UNUSED TimerHandle_t handle) {
  s_metric_timer_cb();
}

static TimerHandle_t prv_metric_timer_init(const char *const pcTimerName,
                                           TickType_t xTimerPeriodInTicks,
                                           UBaseType_t uxAutoReload,
                                           void * pvTimerID,
                                           TimerCallbackFunction_t pxCallbackFunction) {
#if MEMFAULT_FREERTOS_PORT_USE_STATIC_ALLOCATION != 0
  static StaticTimer_t s_metric_timer_context;
  return xTimerCreateStatic(pcTimerName, xTimerPeriodInTicks, uxAutoReload,
                            pvTimerID, pxCallbackFunction, &s_metric_timer_context);
#else
  return xTimerCreate(pcTimerName, xTimerPeriodInTicks, uxAutoReload,
                      pvTimerID, pxCallbackFunction);
#endif
}

bool memfault_platform_metrics_timer_boot(uint32_t period_sec,
                                          MemfaultPlatformTimerCallback callback) {
  TimerHandle_t timer = prv_metric_timer_init("metric_timer",
                                              pdMS_TO_TICKS(period_sec * 1000),
                                              pdTRUE, /* auto reload */
                                              (void*)NULL,
                                              prv_metric_timer_callback);
  if (timer == 0) {
    return false;
  }

  s_metric_timer_cb = callback;

  xTimerStart(timer, 0);
  return true;
}
