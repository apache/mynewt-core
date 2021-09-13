#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Some globals we use to create different types of crashes from the memfault_demo_core.c CLI
//! commands. We put the variables in a separate file / compilation unit to prevent the compiler
//! from detecting / error'ing out on the crashes we are creating for test purposes.

#ifdef __cplusplus
extern "C" {
#endif

extern void *g_memfault_unaligned_buffer;
extern void (*g_bad_func_call)(void);

#ifdef __cplusplus
}
#endif
