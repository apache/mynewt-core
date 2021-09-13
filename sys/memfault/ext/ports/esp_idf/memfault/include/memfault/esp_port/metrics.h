#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!

#include "memfault/metrics/platform/timer.h"

#ifdef __cplusplus
extern "C" {
#endif

//! Called each time a heartbeat interval expires to invoke the handler
//!
//! The Memfault port implements a default implementation of this in
//! ports/esp_idf/memfault/common/memfault_platform_metrics.c which
//! runs the callback on the esp timer task.
//!
//! Since the duty cycle is low (once / hour) and the work performed is
//! copying a small amount of data in RAM, it's recommended to use the default
//! implementation. However, the function is defined as weak so an end-user
//! can override this behavior by implementing the function in their main application.
//!
//! @param handler The callback to invoke to serialize heartbeat metrics
void memfault_esp_metric_timer_dispatch(MemfaultPlatformTimerCallback handler);

#ifdef __cplusplus
}
#endif
