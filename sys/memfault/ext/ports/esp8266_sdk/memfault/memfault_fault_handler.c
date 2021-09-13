//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Logic for wiring up coredump collection to ESP8266 panics.

#include "memfault/panics/arch/xtensa/xtensa.h"
#include "memfault/panics/coredump.h"
#include "memfault/esp8266_port/core.h"

#include "FreeRTOS.h"
#include "task.h"

typedef struct {
  uint32_t exit;
  uint32_t pc;
  uint32_t ps;
  uint32_t a0;
  uint32_t a1;
  uint32_t a2;
  uint32_t a3;
  uint32_t a4;
  uint32_t a5;
  uint32_t a6;
  uint32_t a7;
  uint32_t a8;
  uint32_t a9;
  uint32_t a10;
  uint32_t a11;
  uint32_t a12;
  uint32_t a13;
  uint32_t a14;
  uint32_t a15;
  uint32_t sar;
  uint32_t exccause;
} XtExcFrame;

// Note: The esp8266 SDK implements abort which will invoke the esp-idf coredump handler as well
// as a chip reboot so we just utilize that.

// NB: Disable optimizations so we can get better unwinds from aborts at this point
MEMFAULT_NO_OPT
void memfault_fault_handling_assert(void *pc, void *lr) { abort(); }

MEMFAULT_NO_OPT
void memfault_fault_handling_assert_extra(void *pc, void *lr, sMemfaultAssertInfo *extra_info) {
  abort();
}

// We wrap the panicHandler so a coredump can be captured when a reset takes place:
//  https://github.com/espressif/ESP8266_RTOS_SDK/blob/v3.3/components/freertos/port/esp8266/panic.c#L160-L187
void __real_panicHandler(void *frame, int wdt);

void __wrap_panicHandler(void *frame, int wdt) {
  static bool s_coredump_save_in_progress = false;

  extern int _chip_nmi_cnt;
  _chip_nmi_cnt = 0;

  vPortEnterCritical();
  do {
    REG_WRITE(INT_ENA_WDEV, 0);
  } while (REG_READ(INT_ENA_WDEV) != 0);

  // if we panic'd while trying to save a coredump, we will skip trying again!
  if (!s_coredump_save_in_progress) {
    s_coredump_save_in_progress = true;

    const XtExcFrame *fp = (XtExcFrame *)frame;

    // Clear "EXCM" bit so we don't have to correct PS.OWB to get a good unwind. This will also be
    // more reflective of the state of the register prior to the "panicHandler" being invoked
    const uint32_t corrected_ps = fp->ps & ~(PS_EXCM_MASK);

    sMfltRegState regs = {
      .collection_type = (uint32_t)kMemfaultEsp32RegCollectionType_Lx106,
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
#if XCHAL_HAVE_LOOPS
      .lbeg = fp->lbeg,
      .lend = fp->lend,
      .lcount = fp->lcount,
#endif
      .exccause = fp->exccause,
      .excvaddr = 0,
    };
    memfault_fault_handler(&regs, kMfltRebootReason_HardFault);
  } else {
    MEMFAULT_ESP_PANIC_PRINTF("Exception while saving coredump!");
  }

  // Now that we have saved a coredump, fall into the regular handler
  // which will eventually reboot the system
  __real_panicHandler(frame, wdt);
}
