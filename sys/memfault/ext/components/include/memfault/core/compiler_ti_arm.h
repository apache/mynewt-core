#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Wrappers for common macros & compiler specifics
//!
//! Note: This file should never be included directly but rather picked up through the compiler.h
//! header

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

#define MEMFAULT_PACKED __attribute__((packed))
#define MEMFAULT_PACKED_STRUCT struct MEMFAULT_PACKED
#define MEMFAULT_NORETURN __attribute__((noreturn))
#define MEMFAULT_NAKED_FUNC __attribute__((naked))
#define MEMFAULT_UNREACHABLE
#define MEMFAULT_NO_OPT


#define MEMFAULT_PUT_IN_SECTION(x) __attribute__((section(x)))
#define MEMFAULT_ALIGNED(x) __attribute__((aligned(x)))
#define MEMFAULT_UNUSED __attribute__((unused))
#define MEMFAULT_USED __attribute__((used))
#define MEMFAULT_WEAK __attribute__((weak))
#define MEMFAULT_PRINTF_LIKE_FUNC(a, b) __attribute__ ((format (printf, a, b)))

#define MEMFAULT_CLZ(a) ((a == 0) ? 32UL : (uint32_t)__clz(a))

// Compiler incorrectly thinks return value is missing for pure asm function
// disable the check for them
#pragma diag_push
#pragma diag_suppress 994

#pragma FUNC_CANNOT_INLINE(memfault_get_pc)
MEMFAULT_NAKED_FUNC
static void * memfault_get_pc(void) {
  __asm(" mov r0, lr \n"
        " bx lr");
}

#pragma FUNC_CANNOT_INLINE(__get_PSP)
MEMFAULT_NAKED_FUNC
static uint32_t __get_PSP(void) {
  __asm("   mrs r0, psp\n"
        "   bx lr");
}

#pragma diag_pop

//! __builtin_return_address() currently always returns no value (0) but
//! there is a ticket tracking a real implementation:
//!   https://sir.ext.ti.com/jira/browse/EXT_EP-9303
#define MEMFAULT_GET_LR(_a) _a = __builtin_return_address(0)

//! TI Compiler has an intrinsic __curpc() but it does not work when compiling against Cortex-M
//! variants (i.e --silicon_version=7M4)
#define MEMFAULT_GET_PC(_a)  _a = memfault_get_pc();

#define MEMFAULT_BREAKPOINT(val) __asm(" bkpt #"#val)

#if defined(__cplusplus)
#  define MEMFAULT_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#  define MEMFAULT_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

#ifdef __cplusplus
}
#endif
