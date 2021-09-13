#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Internal utilities used for trace event module

#ifdef __cplusplus
extern "C" {
#endif

//! Resets the Trace Event storage for unit testing purposes.
void memfault_trace_event_reset(void);

#ifdef __cplusplus
}
#endif
