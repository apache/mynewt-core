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
#define MEMFAULT_NORETURN __declspec(noreturn)
#define MEMFAULT_NAKED_FUNC __asm
#define MEMFAULT_UNREACHABLE while (1)
#define MEMFAULT_NO_OPT
#define MEMFAULT_ALIGNED(x) __attribute__((aligned(x)))
#define MEMFAULT_UNUSED __attribute__((unused))
#define MEMFAULT_USED __attribute__((used))
#define MEMFAULT_WEAK __attribute__((weak))
#define MEMFAULT_PRINTF_LIKE_FUNC(a, b)
#define MEMFAULT_CLZ(a) __clz(a)


#define MEMFAULT_GET_LR(_a) _a = ((void *)__return_address())
#define MEMFAULT_GET_PC(_a) _a = (void *)__current_pc()
#define MEMFAULT_PUT_IN_SECTION(x) __attribute__((section(x), zero_init))
#define MEMFAULT_BREAKPOINT(val) __breakpoint(val)

#define MEMFAULT_STATIC_ASSERT(expr, msg) \
    enum {MEMFAULT_CONCAT(MEMFAULT_ASSERTION_AT_, __LINE__) = sizeof(char[(expr) ? 1 : -1])}

#ifdef __cplusplus
}
#endif
