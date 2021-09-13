#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! APIs used internally by the SDK for trace event collection. An user
//! of the SDK should never have to call these routines directly.

#include "memfault/core/trace_reason_user.h"
#include "memfault/core/compiler.h"

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

int memfault_trace_event_capture(eMfltTraceReasonUser reason, void *pc_addr, void *lr_addr);
int memfault_trace_event_with_status_capture(
    eMfltTraceReasonUser reason, void *pc_addr, void *lr_addr, int32_t status);

MEMFAULT_PRINTF_LIKE_FUNC(4, 5)
int memfault_trace_event_with_log_capture(
    eMfltTraceReasonUser reason, void *pc_addr, void *lr_addr, const char *fmt, ...);

int memfault_trace_event_with_compact_log_capture(
    eMfltTraceReasonUser reason, void *lr_addr,
    uint32_t log_id, uint32_t fmt, ...);

#ifdef __cplusplus
}
#endif
