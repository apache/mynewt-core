//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Glue between Zephyr Fault Handler and Memfault Fault Handler for ARM:
//!  https://github.com/zephyrproject-rtos/zephyr/blob/v2.0-branch/arch/arm/core/cortex_m/fault.c#L807

#include <zephyr.h>
#include <init.h>

#include "memfault/core/compiler.h"
#include "memfault/core/reboot_reason_types.h"
#include "memfault/panics/arch/arm/cortex_m.h"
#include "memfault/panics/coredump.h"

#include "zephyr_release_specific_headers.h"

void memfault_platform_reboot(void) {
  const int reboot_type_unused = 0; // ignored for ARM
  sys_reboot(reboot_type_unused);
  MEMFAULT_UNREACHABLE;
}

// By default, the Zephyr NMI handler is an infinite loop. Instead
// let's register the Memfault Exception Handler
static int prv_install_nmi_handler(struct device *dev) {
  extern void z_NmiHandlerSet(void (*pHandler)(void));
  extern void NMI_Handler(void);
  z_NmiHandlerSet(NMI_Handler);
  return 0;
}

SYS_INIT(prv_install_nmi_handler, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

void k_sys_fatal_error_handler(unsigned int reason,
                               const z_arch_esf_t *esf) {
  sMfltRegState reg = {
    .exception_frame = esf->exception_frame_addr,
    .r4 = esf->callee_regs->r4,
    .r5 = esf->callee_regs->r5,
    .r6 = esf->callee_regs->r6,
    .r7 = esf->callee_regs->r7,
    .r8 = esf->callee_regs->r8,
    .r9 = esf->callee_regs->r9,
    .r10 = esf->callee_regs->r10,
    .r11 = esf->callee_regs->r11,
    .exc_return = esf->callee_regs->exc_return,
  };
  memfault_fault_handler(&reg, kMfltRebootReason_HardFault);
}
