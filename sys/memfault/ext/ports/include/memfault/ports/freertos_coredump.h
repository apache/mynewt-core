#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#include <stddef.h>

#include "memfault/config.h"
#include "memfault/panics/platform/coredump.h"

#ifdef __cplusplus
extern "C" {
#endif

//! For each task tracked we will need to collect the TCB + a region of the stack
#define MEMFAULT_PLATFORM_MAX_TASK_REGIONS \
  (MEMFAULT_PLATFORM_MAX_TRACKED_TASKS * (1 /* TCB */ + 1 /* stack */))

//! Helper to collect minimal RAM needed for backtraces of non-running FreeRTOS tasks
//!
//! @param[out] regions Populated with the regions that need to be collected in order
//!  for task and stack state to be recovered for non-running FreeRTOS tasks
//! @param[in] num_regions The number of entries in the 'regions' array
//!
//! @return The number of entries that were populated in the 'regions' argument. Will always
//!  be <= num_regions
size_t memfault_freertos_get_task_regions(sMfltCoredumpRegion *regions, size_t num_regions);

#ifdef __cplusplus
}
#endif
