#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Assert implementation used internally by the Memfault SDK.
//!
//! Asserts are _only_ used for API misuse and configuration issues (i.e a NULL function pointer
//! as for a function in a *StorageImpl context).

#include "memfault/config.h"
#include "memfault/core/compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MEMFAULT_SDK_ASSERT_ENABLED

//! A user of the SDK may optionally override the default assert macro by adding a CFLAG such as:
//! -DMEMFAULT_SDK_ASSERT=MY_PLATFORM_ASSERT
#ifndef MEMFAULT_SDK_ASSERT

//! An end user can override the implementation to trigger a fault/reboot the system.
//!
//! @note The default implementation is a weak function that is a while (1) {} loop.
MEMFAULT_NORETURN void memfault_sdk_assert_func_noreturn(void);

//! Handler that is invoked when the MEMFULT_SDK_ASSERT() check fails
//!
//! @note The handler:
//!  - logs the return address that tripped the assert
//!  - halts the platform by calling "memfault_platform_halt_if_debugging"
//!  - calls memfault_sdk_assert_func_noreturn
void memfault_sdk_assert_func(void);

#define MEMFAULT_SDK_ASSERT(expr) ((expr) ? (void)0 : memfault_sdk_assert_func())

#endif /* MEMFAULT_SDK_ASSERT */

#else

#define MEMFAULT_SDK_ASSERT(expr) (void)0

#endif /* MEMFAULT_SDK_ASSERT_ENABLED */

#ifdef __cplusplus
}
#endif
