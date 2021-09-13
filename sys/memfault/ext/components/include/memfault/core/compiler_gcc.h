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

#define MEMFAULT_PACKED __attribute__((packed))
#define MEMFAULT_PACKED_STRUCT struct MEMFAULT_PACKED
#define MEMFAULT_NORETURN __attribute__((noreturn))
#define MEMFAULT_NAKED_FUNC __attribute__((naked))
#define MEMFAULT_UNREACHABLE __builtin_unreachable()
#if defined(__clang__)
#define MEMFAULT_NO_OPT __attribute__((optnone))
#else
#define MEMFAULT_NO_OPT __attribute__((optimize("O0")))
#endif

#define MEMFAULT_ALIGNED(x) __attribute__((aligned(x)))
#define MEMFAULT_UNUSED __attribute__((unused))
#define MEMFAULT_USED __attribute__((used))
#define MEMFAULT_WEAK __attribute__((weak))
#define MEMFAULT_PRINTF_LIKE_FUNC(a, b) __attribute__ ((format (printf, a, b)))

//! From https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html,
//!  If x is 0, the result is undefined.
#define MEMFAULT_CLZ(a) ((a == 0) ? 32UL : (uint32_t)__builtin_clz(a))

#if defined(__arm__)
#  define MEMFAULT_GET_LR(_a) _a = __builtin_return_address(0)
#  define MEMFAULT_GET_PC(_a) __asm volatile ("mov %0, pc" : "=r" (_a))
#  define MEMFAULT_BREAKPOINT(val) __asm volatile ("bkpt "#val)
#elif defined(__XTENSA__)
#  define MEMFAULT_GET_LR(_a) _a = __builtin_return_address(0)
#  define MEMFAULT_GET_PC(_a) _a = ({ __label__ _l; _l: &&_l;});
#  define MEMFAULT_BREAKPOINT(val)  __asm__ ("break 0,0")
#elif defined(MEMFAULT_UNITTEST) || defined(__APPLE__)  // Memfault iOS SDK also #includes this header
#  define MEMFAULT_GET_LR(_a) _a = 0
#  define MEMFAULT_GET_PC(_a) _a = 0
#  define MEMFAULT_BREAKPOINT(val)  (void)0
#else
#  define MEMFAULT_GET_LR(_a) _a = __builtin_return_address(0)
// Take advantage of "Locally Declared Labels" to get a PC
//   https://gcc.gnu.org/onlinedocs/gcc/Local-Labels.html#Local-Labels
#  define MEMFAULT_GET_PC(_a) _a = ({ __label__ _l; _l: &&_l;});
#  define MEMFAULT_BREAKPOINT(val)  (void)0
#endif /* defined(__GNUC__) && defined(__arm__) */

#if defined(__APPLE__) && defined(MEMFAULT_UNITTEST)
// NB: OSX linker has slightly different placement syntax and requirements
//
// Notably,
// 1. Comma seperated where first item is top level section (i.e __DATA), i.e
//   __attribute__((section("__DATA," x)))
// 2. total length of name must be between 1-16 characters
//
// For now we just stub it out since unit tests don't make use of section locations.
#  define MEMFAULT_PUT_IN_SECTION(x)
#else
#  define MEMFAULT_PUT_IN_SECTION(x) __attribute__((section(x)))
#endif

#if defined(__cplusplus)
#  define MEMFAULT_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#  define MEMFAULT_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

#ifdef __cplusplus
}
#endif
