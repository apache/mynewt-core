#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! ESP32 specific aspects of panic handling

#include <stdint.h>

#include "memfault/core/compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  // A complete dump of all the registers
  kMemfaultEsp32RegCollectionType_Full = 0,
  // A collection of only the active register window
  kMemfaultEsp32RegCollectionType_ActiveWindow = 1,
  // ESP8266 (Tensilica LX106 Core) register collection variant
  kMemfaultEsp32RegCollectionType_Lx106 = 2,
} eMemfaultEsp32RegCollectionType;

//! Register State collected for ESP32 when a fault occurs
MEMFAULT_PACKED_STRUCT MfltRegState {
  uint32_t collection_type; // eMemfaultEsp32RegCollectionType
  // NOTE: This matches the layout expected for kMemfaultEsp32RegCollectionType_ActiveWindow
  uint32_t pc;
  uint32_t ps;
  // NB: The ESP32 has 64 "Address Registers" (ARs) across 4 register windows. Upon exception
  // entry all inactive register windows are force spilled to the stack by software. Therefore, we
  // only need to save the active windows registers at exception entry (referred to as a0-a15).
  uint32_t a[16];
  uint32_t sar;
  uint32_t lbeg;
  uint32_t lend;
  uint32_t lcount;
  uint32_t exccause;
  uint32_t excvaddr;
};


#ifdef __cplusplus
}
#endif
