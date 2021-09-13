#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! aarch64 specific aspects of panic handling.

#include <stdint.h>

#include "memfault/core/compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

//! Note: With aarch64, it is the programmer's responsiblity to store an exception frame
//! so there is no standard layout.
MEMFAULT_PACKED_STRUCT MfltRegState {
  uint64_t sp; // sp prior to exception entry
  uint64_t pc; // pc prior to exception entry
  // Note: The CPSR is a 32 bit register but storing as a 64 bit register to keep packing simple
  uint64_t cpsr;
  uint64_t x[31]; // x0 - x30
};

#ifdef __cplusplus
}
#endif
