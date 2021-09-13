//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#include <stdint.h>

#include "memfault/core/arch.h"
#include "memfault/core/compiler.h"
#include "memfault/core/platform/core.h"

#if MEMFAULT_COMPILER_ARM

bool memfault_arch_is_inside_isr(void) {
  // We query the "Interrupt Control State Register" to determine
  // if there is an active Exception Handler
  volatile uint32_t *ICSR = (uint32_t *)0xE000ED04;
  // Bottom byte makes up "VECTACTIVE"
  return ((*ICSR & 0xff) != 0x0);
}

MEMFAULT_WEAK
void memfault_platform_halt_if_debugging(void) {
  volatile uint32_t *dhcsr = (volatile uint32_t *)0xE000EDF0;
  const uint32_t c_debugen_mask = 0x1;

  if ((*dhcsr & c_debugen_mask) == 0) {
    // no debugger is attached so return since issuing a breakpoint instruction would trigger a
    // fault
    return;
  }

  // NB: A breakpoint with value 'M' (77) for easy disambiguation from other breakpoints that may
  // be used by the system.
  MEMFAULT_BREAKPOINT(77);
}

#endif /* MEMFAULT_COMPILER_ARM */
