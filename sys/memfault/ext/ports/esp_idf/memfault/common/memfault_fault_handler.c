//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Hooks


#include <stdlib.h>

#include "esp_core_dump.h"
#include "freertos/xtensa_api.h"
#include "memfault/panics/arch/xtensa/xtensa.h"
#include "memfault/panics/coredump.h"
#include "memfault/panics/fault_handling.h"

#ifndef ESP_PLATFORM
#  error "The port assumes the esp-idf is in use!"
#endif

// Note: The esp-idf implements abort which will invoke the esp-idf coredump handler as well as a
// chip reboot so we just piggback off of that
void memfault_fault_handling_assert(void *pc, void *lr) { abort(); }

void memfault_fault_handling_assert_extra(void *pc, void *lr, sMemfaultAssertInfo *extra_info) {
  abort();
}

//! Invoked when a panic is detected in the esp-idf when coredumps are enabled
//!
//! @note This requires the following sdkconfig options:
//!   CONFIG_ESP32_ENABLE_COREDUMP=y
//!   CONFIG_ESP32_ENABLE_COREDUMP_TO_FLASH=y
//!
//! @note This is a drop in replacement for the pre-existing flash coredump handler.
//! The default implementation is replaced by leveraging GCCs --wrap feature
//!    https://github.com/espressif/esp-idf/blob/v4.0/components/esp32/panic.c#L620
void __wrap_esp_core_dump_to_flash(XtExcFrame *fp) {
  // Clear "EXCM" bit so we don't have to correct PS.OWB to get a good unwind This will also be
  // more reflective of the state of the register prior to the "panicHandler" being invoked
  const uint32_t corrected_ps = fp->ps & ~(PS_EXCM_MASK);

  sMfltRegState regs = {
    .collection_type = (uint32_t)kMemfaultEsp32RegCollectionType_ActiveWindow,
    .pc = fp->pc,
    .ps = corrected_ps,
    .a = {
      fp->a0,
      fp->a1,
      fp->a2,
      fp->a3,
      fp->a4,
      fp->a5,
      fp->a6,
      fp->a7,
      fp->a8,
      fp->a9,
      fp->a10,
      fp->a11,
      fp->a12,
      fp->a13,
      fp->a14,
      fp->a15,
    },
    .sar = fp->sar,
    .lbeg = fp->lbeg,
    .lend = fp->lend,
    .lcount = fp->lcount,
    .exccause = fp->exccause,
    .excvaddr = fp->excvaddr,
  };

  memfault_fault_handler(&regs, kMfltRebootReason_HardFault);
}
