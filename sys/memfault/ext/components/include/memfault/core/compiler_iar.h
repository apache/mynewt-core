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

#define MEMFAULT_PACKED __packed
#define MEMFAULT_PACKED_STRUCT MEMFAULT_PACKED struct
#define MEMFAULT_NORETURN __noreturn
#define MEMFAULT_NAKED_FUNC
#define MEMFAULT_UNREACHABLE while (1)
#define MEMFAULT_NO_OPT _Pragma("optimize=none")
#define MEMFAULT_ALIGNED(x) _Pragma(MEMFAULT_QUOTE(data_alignment=x))
#define MEMFAULT_UNUSED
#define MEMFAULT_USED __root
#define MEMFAULT_WEAK __weak
#define MEMFAULT_PRINTF_LIKE_FUNC(a, b)
#define MEMFAULT_CLZ(a) __iar_builtin_CLZ(a)


#define MEMFAULT_GET_LR(_a) __asm volatile ("mov %0, lr" : "=r" (_a))
#define MEMFAULT_GET_PC(_a) __asm volatile ("mov %0, pc" : "=r" (_a))
#define MEMFAULT_PUT_IN_SECTION(x) _Pragma(MEMFAULT_QUOTE(location=x))
#define MEMFAULT_BREAKPOINT(val) __asm volatile ("bkpt %0" : : "i"(val))

#define MEMFAULT_STATIC_ASSERT(cond, msg) static_assert(cond, msg)

#ifdef __cplusplus
}
#endif
