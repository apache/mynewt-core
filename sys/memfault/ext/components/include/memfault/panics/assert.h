#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Hooks for linking system assert infra with Memfault error collection

#include "memfault/core/compiler.h"
#include "memfault/core/reboot_reason_types.h"
#include "memfault/panics/fault_handling.h"

#ifdef __cplusplus
extern "C" {
#endif

//! The 'MEMFAULT_ASSERT*' hooks that should be called as part of the normal ASSERT flow in
//! the system. This will save the reboot info to persist across boots.
//!
//! - MEMFAULT_ASSERT : normal assert
//! - MEMFAULT_ASSERT_EXTRA : assert with a single arbitrary uint32_t context value included
//! - MEMFAULT_ASSERT_EXTRA_AND_REASON : assert with arbitrary uint32_t value and reason code
//! - MEMFAULT_SOFTWARE_WATCHDOG : assert with kMfltRebootReason_SoftwareWatchdog reason set
//!
//! NB: We may also want to think about whether we should save off something like __LINE__ (or use a
//! compiler flag) that blocks the compiler from coalescing asserts (since that could be confusing
//! to an end customer trying to figure out the exact assert hit)

#define MEMFAULT_ASSERT_EXTRA_AND_REASON(_extra, _reason)        \
  do {                                                           \
    void *pc;                                                    \
    MEMFAULT_GET_PC(pc);                                         \
    void *lr;                                                    \
    MEMFAULT_GET_LR(lr);                                         \
    sMemfaultAssertInfo _assert_info = {                         \
      .extra = (uint32_t)_extra,                                 \
      .assert_reason = _reason,                                  \
    };                                                           \
    memfault_fault_handling_assert_extra(pc, lr, &_assert_info); \
  } while (0)

#define MEMFAULT_ASSERT_RECORD(_extra) \
  MEMFAULT_ASSERT_EXTRA_AND_REASON(kMfltRebootReason_Assert, _extra)

#define MEMFAULT_ASSERT_EXTRA(exp, _extra) \
  do {                                     \
    if (!(exp)) {                          \
      MEMFAULT_ASSERT_RECORD(_extra);      \
    }                                      \
  } while (0)

#define MEMFAULT_ASSERT(exp)                  \
  do {                                        \
    if (!(exp)) {                             \
      void *pc;                               \
      MEMFAULT_GET_PC(pc);                    \
      void *lr;                               \
      MEMFAULT_GET_LR(lr);                    \
      memfault_fault_handling_assert(pc, lr); \
    }                                         \
  } while (0)

//! Assert subclass to be used when a software watchdog trips.
#define MEMFAULT_SOFTWARE_WATCHDOG() \
  MEMFAULT_ASSERT_EXTRA_AND_REASON(0, kMfltRebootReason_SoftwareWatchdog)

#ifdef __cplusplus
}
#endif
