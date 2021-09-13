#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Cortex-M specific aspects of panic handling.

#include <stdint.h>

#include "memfault/core/compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

// Registers auto-stacked as part of Cortex-M exception entry
typedef MEMFAULT_PACKED_STRUCT MfltExceptionFrame {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t pc;
  uint32_t xpsr;
} sMfltExceptionFrame;

// Register State collected for Cortex-M at exception entry
MEMFAULT_PACKED_STRUCT MfltRegState {
  // Exception-entry value of the stack pointer that was active when the fault occurred (i.e msp or
  // psp). This is where the hardware will automatically stack caller-saved register state
  // (https://mflt.io/2rejx7A) as part of the exception entry flow. This address is guaranteed to
  // match the layout of the "sMfltExceptionFrame".
  sMfltExceptionFrame *exception_frame;
  // callee saved registers
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
  uint32_t exc_return; // on exception entry, this value is in the LR
};

#ifdef __cplusplus
}
#endif
