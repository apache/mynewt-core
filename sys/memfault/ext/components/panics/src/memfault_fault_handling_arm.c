//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Fault handling for Cortex M based devices

#include "memfault/core/compiler.h"

#if MEMFAULT_COMPILER_ARM

#include "memfault/panics/fault_handling.h"

#include "memfault/core/platform/core.h"
#include "memfault/core/reboot_tracking.h"
#include "memfault/panics/arch/arm/cortex_m.h"
#include "memfault/panics/coredump.h"
#include "memfault/panics/coredump_impl.h"

static eMemfaultRebootReason s_crash_reason = kMfltRebootReason_Unknown;

typedef MEMFAULT_PACKED_STRUCT MfltCortexMRegs {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
  uint32_t r12;
  uint32_t sp;
  uint32_t lr;
  uint32_t pc;
  uint32_t psr;
  uint32_t msp;
  uint32_t psp;
} sMfltCortexMRegs;

size_t memfault_coredump_storage_compute_size_required(void) {
  // actual values don't matter since we are just computing the size
  sMfltCortexMRegs core_regs = { 0 };
  sMemfaultCoredumpSaveInfo save_info = {
    .regs = &core_regs,
    .regs_size = sizeof(core_regs),
    .trace_reason = kMfltRebootReason_UnknownError,
  };

  sCoredumpCrashInfo info = {
    // we'll just pass the current stack pointer, value shouldn't matter
    .stack_address = (void *)&core_regs,
    .trace_reason = save_info.trace_reason,
    .exception_reg_state = NULL,
  };
  save_info.regions = memfault_platform_coredump_get_regions(&info, &save_info.num_regions);

  return memfault_coredump_get_save_size(&save_info);
}

#if defined(__CC_ARM)

static uint32_t prv_read_psp_reg(void) {
  register uint32_t reg_val  __asm("psp");
  return reg_val;
}

static uint32_t prv_read_msp_reg(void) {
  register uint32_t reg_val __asm("msp");
  return reg_val;
}

#elif defined(__TI_ARM__)

static uint32_t prv_read_psp_reg(void) {
  return __get_PSP();
}

static uint32_t prv_read_msp_reg(void) {
  return __get_MSP();
}

#elif defined(__GNUC__) || defined(__clang__) || defined(__ICCARM__)

static uint32_t prv_read_psp_reg(void) {
  uint32_t reg_val;
  __asm volatile ("mrs %0, psp"  : "=r" (reg_val));
  return reg_val;
}

static uint32_t prv_read_msp_reg(void) {
  uint32_t reg_val;
  __asm volatile ("mrs %0, msp"  : "=r" (reg_val));
  return reg_val;
}

#else
#  error "New compiler to add support for!"
#endif

#if !MEMFAULT_PLATFORM_FAULT_HANDLER_CUSTOM
MEMFAULT_WEAK
void memfault_platform_fault_handler(MEMFAULT_UNUSED const sMfltRegState *regs,
                                     MEMFAULT_UNUSED eMemfaultRebootReason reason) {
}
#endif /* MEMFAULT_PLATFORM_FAULT_HANDLER_CUSTOM */

void memfault_fault_handler(const sMfltRegState *regs, eMemfaultRebootReason reason) {
  memfault_platform_fault_handler(regs, reason);

  if (s_crash_reason == kMfltRebootReason_Unknown) {
    sMfltRebootTrackingRegInfo info = {
      .pc = regs->exception_frame->pc,
      .lr = regs->exception_frame->lr,
    };
    memfault_reboot_tracking_mark_reset_imminent(reason, &info);
    s_crash_reason = reason;
  }

  const bool fpu_stack_space_rsvd = ((regs->exc_return & (1 << 4)) == 0);
  const bool stack_alignment_forced = ((regs->exception_frame->xpsr & (1 << 9)) != 0);

  uint32_t sp_prior_to_exception =
      (uint32_t)regs->exception_frame + (fpu_stack_space_rsvd ? 0x68 : 0x20);

  if (stack_alignment_forced) {
    sp_prior_to_exception += 0x4;
  }

  // Read the "SPSEL" bit where
  //  0 = Main Stack Pointer in use prior to exception
  //  1 = Process Stack Pointer in use prior to exception
  const bool msp_was_active = (regs->exc_return & (1 << 2)) == 0;

  sMfltCortexMRegs core_regs = {
    .r0 = regs->exception_frame->r0,
    .r1 = regs->exception_frame->r1,
    .r2 = regs->exception_frame->r2,
    .r3 = regs->exception_frame->r3,
    .r4 = regs->r4,
    .r5 = regs->r5,
    .r6 = regs->r6,
    .r7 = regs->r7,
    .r8 = regs->r8,
    .r9 = regs->r9,
    .r10 = regs->r10,
    .r11 = regs->r11,
    .r12 = regs->exception_frame->r12,
    .sp = sp_prior_to_exception,
    .lr = regs->exception_frame->lr,
    .pc = regs->exception_frame->pc,
    .psr = regs->exception_frame->xpsr,
    .msp = msp_was_active ? sp_prior_to_exception : prv_read_msp_reg(),
    .psp = !msp_was_active ? sp_prior_to_exception : prv_read_psp_reg(),
  };

  sMemfaultCoredumpSaveInfo save_info = {
    .regs = &core_regs,
    .regs_size = sizeof(core_regs),
    .trace_reason = s_crash_reason,
  };

  sCoredumpCrashInfo info = {
    .stack_address = (void *)sp_prior_to_exception,
    .trace_reason = save_info.trace_reason,
    .exception_reg_state = regs,
  };
  save_info.regions = memfault_platform_coredump_get_regions(&info, &save_info.num_regions);

  const bool coredump_saved = memfault_coredump_save(&save_info);
  if (coredump_saved) {
    memfault_reboot_tracking_mark_coredump_saved();
  }

  memfault_platform_reboot();
  MEMFAULT_UNREACHABLE;
}



// The fault handling shims below figure out what stack was being used leading up to the exception,
// build the sMfltRegState argument and pass that as well as the reboot reason to memfault_fault_handler


#if defined(__CC_ARM)

// armcc emits a define for the CPU target.
//
// Use that information to decide whether or not to pick up the ARMV6M port by default
//
// Cortex M0 (--cpu=cortex-m0)
//   __TARGET_CPU_CORTEX_M0
// Cortex M0+ (--cpu=cortex-m0plus or --cpu=cortex-m0+)
//   __TARGET_CPU_CORTEX_M0PLUS
//   __TARGET_CPU_CORTEX_M0_
#if defined(__TARGET_CPU_CORTEX_M0) || defined (__TARGET_CPU_CORTEX_M0_) || defined(__TARGET_CPU_CORTEX_M0PLUS)
#define MEMFAULT_USE_ARMV6M_FAULT_HANDLER 1
#endif

#if !defined(MEMFAULT_USE_ARMV6M_FAULT_HANDLER)
__asm __forceinline void memfault_fault_handling_shim(int reason) {
  extern memfault_fault_handler;
  tst lr, #4
  ite eq
  mrseq r3, msp
  mrsne r3, psp
  push {r3-r11, lr}
  mov r1, r0
  mov r0, sp
  b memfault_fault_handler
  ALIGN
}
#else

__asm __forceinline void memfault_fault_handling_shim(int reason) {
  extern memfault_fault_handler;
  PRESERVE8
  mov r1, lr
  movs r2, #4
  tst  r1,r2
  mrs r12, msp
  beq msp_active_at_crash
  mrs r12, psp
msp_active_at_crash mov r3, r11
  mov r2, r10
  mov r1, r9
  mov r9, r0
  mov r0, r8
  push {r0-r3, lr}
  mov r3, r12
  push {r3-r7}
  mov r0, sp
  mov r1, r9
  ldr r2, =memfault_fault_handler
  bx r2
  ALIGN
}
#endif

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_HARD_FAULT(void) {
  ldr r0, =0x9400 // kMfltRebootReason_HardFault
  ldr r1, =memfault_fault_handling_shim
  bx r1
  ALIGN
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_MEMORY_MANAGEMENT(void) {
  ldr r0, =0x9200 // kMfltRebootReason_MemFault
  ldr r1, =memfault_fault_handling_shim
  bx r1
  ALIGN
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_BUS_FAULT(void) {
  ldr r0, =0x9100 // kMfltRebootReason_BusFault
  ldr r1, =memfault_fault_handling_shim
  bx r1
  ALIGN
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_USAGE_FAULT(void) {
  ldr r0, =0x9300 // kMfltRebootReason_UsageFault
  ldr r1, =memfault_fault_handling_shim
  bx r1
  ALIGN
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_NMI(void) {
  ldr r0, =0x8004 // kMfltRebootReason_Nmi
  ldr r1, =memfault_fault_handling_shim
  bx r1
  ALIGN
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_WATCHDOG(void) {
  ldr r0, =0x8006 // kMfltRebootReason_SoftwareWatchdog
  ldr r1, =memfault_fault_handling_shim
  bx r1
  ALIGN
}

#elif defined(__TI_ARM__)

// Note: 'reason' is passed as arg0. However we mark the function
// as void so the TI compiler does not emit any function prologue
// pushing args on the stack
MEMFAULT_NAKED_FUNC
void memfault_fault_handling_shim(void /* int reason */) {
  __asm(" tst lr, #4 \n"
        " ite eq \n"
        " mrseq r3, msp \n"
        " mrsne r3, psp \n"
        " push {r3-r11, lr} \n"
        " mov r1, r0 \n"
        " mov r0, sp \n"
        " b memfault_fault_handler");
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_HARD_FAULT(void) {
  __asm(" mov r0, #0x9400 \n" // kMfltRebootReason_HardFault
        " b memfault_fault_handling_shim \n");
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_MEMORY_MANAGEMENT(void) {
  __asm(" mov r0, #0x9200 \n" // kMfltRebootReason_MemFault
        " b memfault_fault_handling_shim \n");
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_BUS_FAULT(void) {
  __asm(" mov r0, #0x9100 \n" // kMfltRebootReason_BusFault
        " b memfault_fault_handling_shim \n");
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_USAGE_FAULT(void) {
  __asm(" mov r0, #0x9300 \n" // kMfltRebootReason_UsageFault
        " b memfault_fault_handling_shim \n");
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_NMI(void) {
  __asm(" mov r0, #0x8004 \n" // kMfltRebootReason_Nmi
        " b memfault_fault_handling_shim \n");
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_WATCHDOG(void) {
  __asm(" mov r0, #0x8006 \n" // kMfltRebootReason_SoftwareWatchdog
        " b memfault_fault_handling_shim \n");
}

#elif defined(__GNUC__) || defined(__clang__)

#if defined(__ARM_ARCH) && (__ARM_ARCH == 6)
#define MEMFAULT_USE_ARMV6M_FAULT_HANDLER 1
#endif

// Note: ARMV8-M has a subprofile referred to as the "Baseline" implementation
// with an instruction set similar to ARMV6-M. See https://mflt.io/armv8m-subprofiles
// for more details.
#if defined(__ARM_ARCH_8M_BASE__) && (__ARM_ARCH_8M_BASE__ == 1)
#define MEMFAULT_USE_ARMV8M_BASE_FAULT_HANDLER 1
#endif

#if (!defined(MEMFAULT_USE_ARMV6M_FAULT_HANDLER) && \
     !defined(MEMFAULT_USE_ARMV8M_BASE_FAULT_HANDLER))
#define MEMFAULT_HARDFAULT_HANDLING_ASM(_x)      \
  __asm volatile(                                \
      "tst lr, #4 \n"                            \
      "ite eq \n"                                \
      "mrseq r3, msp \n"                         \
      "mrsne r3, psp \n"                         \
      "push {r3-r11, lr} \n"                     \
      "mov r0, sp \n"                            \
      "ldr r1, =%0 \n"                           \
      "b memfault_fault_handler \n"              \
      :                                          \
      : "i" (_x)                                 \
   )
#else
#define MEMFAULT_HARDFAULT_HANDLING_ASM(_x)      \
  __asm volatile(                                \
      "mov r0, lr \n"                            \
      "movs r1, #4 \n"                           \
      "tst  r0,r1 \n"                            \
      "mrs r12, msp \n"                          \
      "beq msp_active_at_crash_%= \n"            \
      "mrs r12, psp \n"                          \
      "msp_active_at_crash_%=: \n"               \
      "mov r0, r8 \n"                            \
      "mov r1, r9 \n"                            \
      "mov r2, r10 \n"                           \
      "mov r3, r11 \n"                           \
      "push {r0-r3, lr} \n"                      \
      "mov r3, r12 \n"                           \
      "push {r3-r7} \n"                          \
      "mov r0, sp \n"                            \
      "ldr r1, =%c0 \n"                          \
      "b memfault_fault_handler \n"              \
      :                                          \
      : "i" ((uint16_t)_x)                       \
   )
#endif

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_HARD_FAULT(void) {
  MEMFAULT_HARDFAULT_HANDLING_ASM(kMfltRebootReason_HardFault);
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_MEMORY_MANAGEMENT(void) {
  MEMFAULT_HARDFAULT_HANDLING_ASM(kMfltRebootReason_MemFault);
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_BUS_FAULT(void) {
  MEMFAULT_HARDFAULT_HANDLING_ASM(kMfltRebootReason_BusFault);
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_USAGE_FAULT(void) {
  MEMFAULT_HARDFAULT_HANDLING_ASM(kMfltRebootReason_UsageFault);
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_NMI(void) {
  MEMFAULT_HARDFAULT_HANDLING_ASM(kMfltRebootReason_Nmi);
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_WATCHDOG(void) {
  MEMFAULT_HARDFAULT_HANDLING_ASM(kMfltRebootReason_SoftwareWatchdog);
}

#elif defined(__ICCARM__)

#if __ARM_ARCH == 6
#define MEMFAULT_USE_ARMV6M_FAULT_HANDLER 1
#endif

#if !defined(MEMFAULT_USE_ARMV6M_FAULT_HANDLER)
#define MEMFAULT_HARDFAULT_HANDLING_ASM(_x)      \
  __asm volatile(                                \
      "tst lr, #4 \n"                            \
      "ite eq \n"                                \
      "mrseq r3, msp \n"                         \
      "mrsne r3, psp \n"                         \
      "push {r3-r11, lr} \n"                     \
      "mov r0, sp \n"                            \
      "mov r1, %0 \n"                            \
      "b memfault_fault_handler \n"              \
      :                                          \
      : "i" (_x)                                 \
   )

#else

// Note: Below IAR will build the enum value
// as part of the prologue to the asm statement and
// place the value in r0
#define MEMFAULT_HARDFAULT_HANDLING_ASM(_x)      \
  __asm volatile(                                \
      "mov r1, lr \n"                            \
      "movs r2, #4 \n"                           \
      "tst  r1,r2 \n"                            \
      "mrs r12, msp \n"                          \
      "beq msp_active_at_crash \n"               \
      "mrs r12, psp \n"                          \
      "msp_active_at_crash: \n"                  \
      "mov r3, r11 \n"                           \
      "mov r2, r10 \n"                           \
      "mov r1, r9 \n"                            \
      "mov r9, r0 \n"                            \
      "mov r0, r8 \n"                            \
      "push {r0-r3, lr} \n"                      \
      "mov r3, r12 \n"                           \
      "push {r3-r7} \n"                          \
      "mov r0, sp \n"                            \
      "mov r1, r9 \n"                            \
      "ldr r2, =memfault_fault_handler \n"       \
      "bx r2 \n"                                 \
      :                                          \
      : "r" (_x)                                 \
   )

#endif

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_HARD_FAULT(void) {
  MEMFAULT_HARDFAULT_HANDLING_ASM(kMfltRebootReason_HardFault);
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_MEMORY_MANAGEMENT(void) {
  MEMFAULT_HARDFAULT_HANDLING_ASM(kMfltRebootReason_MemFault);
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_BUS_FAULT(void) {
  MEMFAULT_HARDFAULT_HANDLING_ASM(kMfltRebootReason_BusFault);
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_USAGE_FAULT(void) {
  MEMFAULT_HARDFAULT_HANDLING_ASM(kMfltRebootReason_UsageFault);
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_NMI(void) {
  MEMFAULT_HARDFAULT_HANDLING_ASM(kMfltRebootReason_Nmi);
}

MEMFAULT_NAKED_FUNC
void MEMFAULT_EXC_HANDLER_WATCHDOG(void) {
  MEMFAULT_HARDFAULT_HANDLING_ASM(kMfltRebootReason_SoftwareWatchdog);
}

#else
#  error "New compiler to add support for!"
#endif

// The ARM architecture has a reserved instruction that is "Permanently Undefined" and always
// generates an Undefined Instruction exception causing an ARM fault handler to be invoked.
//
// We use this instruction to "trap" into the fault handler logic. We use 'M' (77) as the immediate
// value for easy disambiguation from any other udf invocations in a system.
#if defined(__CC_ARM)
__asm __forceinline void MEMFAULT_ASSERT_TRAP(void) {
  PRESERVE8
  UND #77
  ALIGN
}
#elif defined(__TI_ARM__)
// The TI Compiler doesn't support the udf asm instruction
// so we encode the instruction & a nop as a word literal

#pragma diag_push
#pragma diag_suppress 1119

void MEMFAULT_ASSERT_TRAP(void) {
  __asm(" .word 3204505165"); // 0xbf00de4d
}

#pragma diag_pop
#else
#define MEMFAULT_ASSERT_TRAP() __asm volatile ("udf #77")
#endif

static void prv_fault_handling_assert(void *pc, void *lr, eMemfaultRebootReason reason) {
  sMfltRebootTrackingRegInfo info = {
    .pc = (uint32_t)pc,
    .lr = (uint32_t)lr,
  };
  s_crash_reason = reason;
  memfault_reboot_tracking_mark_reset_imminent(s_crash_reason, &info);

#if MEMFAULT_ASSERT_HALT_IF_DEBUGGING_ENABLED
  memfault_platform_halt_if_debugging();
#endif

  MEMFAULT_ASSERT_TRAP();

  // We just trap'd into the fault handler logic so it should never be possible to get here but if
  // we do the best thing that can be done is rebooting the system to recover it.
  memfault_platform_reboot();
}

// Note: These functions are annotated as "noreturn" which can be useful for static analysis.
// However, this can also lead to compiler optimizations that make recovering local variables
// difficult (such as ignoring ABI requirements to preserve callee-saved registers)
MEMFAULT_NO_OPT
void memfault_fault_handling_assert(void *pc, void *lr) {
  prv_fault_handling_assert(pc, lr, kMfltRebootReason_Assert);
  MEMFAULT_UNREACHABLE;
}
MEMFAULT_NO_OPT
void memfault_fault_handling_assert_extra(void *pc, void *lr, sMemfaultAssertInfo *extra_info) {
  prv_fault_handling_assert(pc, lr, extra_info->assert_reason);
  MEMFAULT_UNREACHABLE;
}

#endif /* MEMFAULT_COMPILER_ARM */
