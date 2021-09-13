#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! This file needs to be included from your platforms FreeRTOSConfig.h to take advantage of
//! Memfault's hooks into the FreeRTOS tracing utilities

#ifdef __cplusplus
extern "C" {
#endif

#include "memfault/config.h"

// FreeRTOSConfig.h is often included in assembly files so wrap function declarations for
// convenience to prevent compilation errors
#if !defined(__ASSEMBLER__) && !defined(__IAR_SYSTEMS_ASM__)

void memfault_freertos_trace_task_create(void *tcb);
void memfault_freertos_trace_task_delete(void *tcb);

#include  "memfault/core/heap_stats.h"

#endif

//
// We ifndef the trace macros so it's possible for an end user to use a custom definition that
// calls Memfault's implementation as well as their own
//

#ifndef traceTASK_CREATE
#define traceTASK_CREATE(pxNewTcb) memfault_freertos_trace_task_create(pxNewTcb)
#endif

#ifndef traceTASK_DELETE
#define traceTASK_DELETE(pxTaskToDelete) memfault_freertos_trace_task_delete(pxTaskToDelete)
#endif

#if MEMFAULT_FREERTOS_PORT_HEAP_STATS_ENABLE

#if MEMFAULT_COREDUMP_HEAP_STATS_LOCK_ENABLE != 0
// FreeRTOS has its own locking mechanism (suspends tasks) so don't attempt
// to use the memfault_lock implementation as well
#error "MEMFAULT_COREDUMP_HEAP_STATS_LOCK_ENABLE must be 0 when using MEMFAULT_FREERTOS_PORT_HEAP_STATS_ENABLE"
#endif

#ifndef traceFREE
#define traceFREE(pv, xBlockSize) MEMFAULT_HEAP_STATS_FREE(pv)
#endif

#ifndef traceMALLOC
#define traceMALLOC(pvReturn, xWantedSize) MEMFAULT_HEAP_STATS_MALLOC(pvReturn, xWantedSize)
#endif

#endif /* MEMFAULT_FREERTOS_PORT_HEAP_STATS_ENABLE */

//! A define that is used to assert that this file has been included from FreeRTOSConfig.h
#define MEMFAULT_FREERTOS_TRACE_ENABLED 1

#ifdef __cplusplus
}
#endif
