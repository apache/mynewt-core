#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#include "memfault/core/compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

//! Counts the number of __VA_ARGS__ (provided the length is <= 32)
//!
//! Runs as early preprocess phase which makes it easy to stringify the result and embed
//! it in something like a static assert:
//!  MEMFAULT_STATIC_ASSERT( <some_check>,
//!    "Too many args: " MEMFAULT_EXPAND_AND_QUOTE(MEMFAULT_ARG_COUNT_UP_TO_32(__VA_ARGS__)))
#define MEMFAULT_ARG_COUNT_UP_TO_32(...)                                \
  _MEMFAULT_ARG_COUNT_UP_TO_32_IMPL(                                    \
  dummy, ##__VA_ARGS__, 32, 31, 30, 29, 28, 27, 26, 25, 24,             \
  23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8,         \
  7, 6, 5, 4, 3, 2, 1, 0)

#define _MEMFAULT_ARG_COUNT_UP_TO_32_IMPL(                              \
    dummy, _32, _31, _30, _29, _28, _27, _26, _25, _24, _23, _22,       \
    _21, _20, _19, _18, _17, _16, _15, _14, _13, _12, _11, _10, _09,    \
    _08, _07, _06, _05, _04, _03, _02, _01, count, ...)                 \
  count

//! Note: We sanity check that "the ‘##’ token paste operator has a special meaning when placed
//! between a comma and a variable argument" (https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html)
//! where
//! ", ##__VA_ARGS__" evaluates to "" if __VA_ARGS__ is empty.
//!
//! While this feature is a GNU extension, it's widely supported in most other modern compilers
//! (ARM Compiler 5, Clang, iccarm). When using GCC this means you should be compiling with
//! -std=gnu11 (default), -std=gnu99, etc, and with other compilers you may need to enable GNU
//! extensions.
//!
//! The check itself is pretty simple.
//!  When supported, "(, ##__VA_ARGS__)" expands to ()
//!  When not supported  "(, ##__VA_ARGS__)" expands to (,)
//! If we count the arguments via a preprocessor macro we will consequently get 0 when the
//! feature is supported and 1 when it is not (due to the ',' being left in the expansion)
//!
//! The C++20 introduced __VA_OPT__ for this behavior but that is still not widely available yet
MEMFAULT_STATIC_ASSERT(MEMFAULT_ARG_COUNT_UP_TO_32() == 0, "## operator not supported, enable gnu extensions on your compiler");

#ifdef __cplusplus
}
#endif
