#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//! @brief
//!
//! A timer API which needs to be implemented to start collecting memfault heartbeat metrics

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! A timer callback used by the Memfault library
typedef void (MemfaultPlatformTimerCallback)(void);

//! Start a repeating timer that invokes 'callback' every period_s
//!
//! @param period_sec The interval (in seconds) to invoke the callback at
//! @param callback to invoke
//!
//! @return true if the timer was successfully created and started, false otherwise
bool memfault_platform_metrics_timer_boot(uint32_t period_sec, MemfaultPlatformTimerCallback *callback);

#ifdef __cplusplus
}
#endif
