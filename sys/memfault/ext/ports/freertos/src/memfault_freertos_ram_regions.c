//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @note This file enables collecting a portion of all the FreeRTOS task state in a minimal RAM
//! footprint. If you are able to collect all of RAM in your coredump, there is no need to use the
//! utilities in this file
//!
//! To utilize this implementation to capture a portion of all the FreeRTOS tasks:
//!
//! 1) Update linker script to place FreeRTOS at a fixed address:
//!
//!    .bss (NOLOAD) :
//!    {
//!        _sbss = . ;
//!        __bss_start__ = _sbss;
//!        __memfault_capture_start = .;
//!         *tasks.o(.bss COMMON .bss*)
//!         *timers*.o(.bss COMMON .bss*)
//!        __memfault_capture_end = .;
//!
//! 2) Add this file to your build and update FreeRTOSConfig.h to
//!    include "memfault/ports/freertos_trace.h"
//!
//! 3) Implement memfault_platform_sanitize_address_range(). This routine is used
//!    to run an extra sanity check in case any task context state is corrupted, i.e
//!
//!     #include "memfault/ports/freertos_coredump.h"
//!     #include "memfault/core/math.h"
//!     // ...
//!     size_t memfault_platform_sanitize_address_range(void *start_addr, size_t desired_size) {
//!       // Note: This will differ depending on the memory map of the MCU
//!       const uint32_t ram_start = 0x20000000;
//!       const uint32_t ram_end = 0x20000000 + 64 * 1024;
//!       if ((uint32_t)start_addr >= ram_start && (uint32_t)start_addr < ram_end) {
//!         return MEMFAULT_MIN(desired_size, ram_end - (uint32_t)start_addr);
//!       }
//!
//!      // invalid address
//!      return 0;
//!     }
//!
//! 4) Update memfault_platform_coredump_get_regions() to include FreeRTOS state
//!    and new regions:
//!
//!   const sMfltCoredumpRegion *memfault_platform_coredump_get_regions(
//!       const sCoredumpCrashInfo *crash_info, size_t *num_regions) {
//!     int region_idx = 0;
//!
//!     const size_t active_stack_size_to_collect = 512;
//!
//!     // first, capture the active stack
//!     s_coredump_regions[0] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(
//!        crash_info->stack_address,
//!        memfault_platform_sanitize_address_range(crash_info->stack_address,
//!          active_stack_size_to_collect));
//!     region_idx++;
//!
//!     extern uint32_t __memfault_capture_start;
//!     extern uint32_t __memfault_capture_end;
//!     const size_t memfault_region_size = (uint32_t)&__memfault_capture_end -
//!         (uint32_t)&__memfault_capture_start;
//!
//!     s_coredump_regions[region_idx] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(
//!        &__memfault_capture_start, memfault_region_size);
//!     region_idx++;
//!
//!     region_idx += memfault_freertos_get_task_regions(&s_coredump_regions[region_idx],
//!         MEMFAULT_ARRAY_SIZE(s_coredump_regions) - region_idx);

#include "memfault/ports/freertos_coredump.h"

#include "memfault/config.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/math.h"
#include "memfault/panics/coredump.h"

#include "FreeRTOS.h"
#include "task.h"

#if !defined(MEMFAULT_FREERTOS_TRACE_ENABLED)
#error "'#include "memfault/ports/freertos_trace.h"' must be added to FreeRTOSConfig.h"
#endif


#if tskKERNEL_VERSION_MAJOR <= 8
//! Note: What we want here is sizeof(TCB_t) but that is a private declaration in FreeRTOS
//! tasks.c. Since the static declaration doesn't exist for FreeRTOS kernel <= 8, fallback to a
//! generous size that will include the entire TCB. A user of the SDK can tune the size by adding
//! MEMFAULT_FREERTOS_TCB_SIZE to memfault_platform_config.h
#if !defined(MEMFAULT_FREERTOS_TCB_SIZE)
#define MEMFAULT_FREERTOS_TCB_SIZE 200
#endif
#else
#define MEMFAULT_FREERTOS_TCB_SIZE sizeof(StaticTask_t)
#endif

//! The maximum number
static void *s_task_tcbs[MEMFAULT_PLATFORM_MAX_TRACKED_TASKS];
#define EMPTY_SLOT 0

static bool prv_find_slot(size_t *idx, void *desired_tcb) {
  for (size_t i = 0; i < MEMFAULT_PLATFORM_MAX_TRACKED_TASKS; i++) {
    if (s_task_tcbs[i] == desired_tcb) {
      *idx = i;
      return true;
    }
  }

  return false;
}

void memfault_freertos_trace_task_create(void *tcb) {
  size_t idx = 0;

  // For a typical workload, tasks are created as part of the boot process and never after
  // the scheduler has been started but we add a critical section to cover the off-chance
  // that two tasks are creating other tasks at exactly the same time.
  portENTER_CRITICAL();
  const bool slot_found = prv_find_slot(&idx, EMPTY_SLOT);
  if (slot_found) {
    s_task_tcbs[idx] = tcb;
  }
  portEXIT_CRITICAL();

  if (!slot_found) {
    MEMFAULT_LOG_ERROR("Task registry full (%d)", MEMFAULT_PLATFORM_MAX_TRACKED_TASKS);
  }
}

void memfault_freertos_trace_task_delete(void *tcb) {
  size_t idx = 0;
  if (!prv_find_slot(&idx, tcb)) {
    // A TCB not currently in the registry
    return;
  }

  // NB: aligned 32 bit writes are atomic and the same task can't be deleted twice
  // so no need for a critical section here
  s_task_tcbs[idx] = EMPTY_SLOT;
}

size_t memfault_freertos_get_task_regions(sMfltCoredumpRegion *regions, size_t num_regions) {
  if (regions == NULL || num_regions == 0) {
    return 0;
  }

  size_t region_idx = 0;

  // First we will try to store all the task TCBs. This way if we run out of space
  // while storing tasks we will still be able to recover the state of all the threads
  for (size_t i = 0; i < MEMFAULT_ARRAY_SIZE(s_task_tcbs) && region_idx < num_regions; i++) {
    void *tcb_address = s_task_tcbs[i];
    if (tcb_address == EMPTY_SLOT) {
      continue;
    }

    const size_t tcb_size = memfault_platform_sanitize_address_range(
        tcb_address, MEMFAULT_FREERTOS_TCB_SIZE);
    if (tcb_size == 0) {
      // An invalid address, scrub the TCB from the list so we don't try to dereference
      // it when grabbing stacks below and move on.
      s_task_tcbs[i] = EMPTY_SLOT;
      continue;
    }

    regions[region_idx] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(tcb_address, tcb_size);
    region_idx++;
  }

  // Now we store the region of the stack where context is saved. This way
  // we can unwind the stacks for threads that are not actively running
  for (size_t i = 0; i < MEMFAULT_ARRAY_SIZE(s_task_tcbs) && region_idx < num_regions; i++) {
    void *tcb_address = s_task_tcbs[i];
    if (tcb_address == EMPTY_SLOT) {
      continue;
    }

    // pxTopOfStack is always the first entry in the FreeRTOS TCB
    void *top_of_stack = (void *)(*(uintptr_t *)tcb_address);
    const size_t stack_size = memfault_platform_sanitize_address_range(
        top_of_stack, MEMFAULT_PLATFORM_TASK_STACK_SIZE_TO_COLLECT);
    if (stack_size == 0) {
      continue;
    }

    regions[region_idx] =  MEMFAULT_COREDUMP_MEMORY_REGION_INIT(top_of_stack, stack_size);
    region_idx++;
    if (region_idx == num_regions) {
      return region_idx;
    }
  }

  return region_idx;
}
