#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Functions that can be optionally overriden by the user of the SDK.
//!
//! Default "weak function" stub definitions are provided for each of these functions
//! in the SDK itself

#ifdef __cplusplus
extern "C" {
#endif

//! Locking APIs used within the Memfault SDK
//!
//! The lock dependencies can (optionally) be implemented by the application to enable locking
//! around accesses to Memfault APIs. This is required if calls are made to Memfault APIs from
//! multiple tasks _and_ tasks do _not_ run to completion (can be interrupted by other tasks).
//!
//! @note By default, a weak version of this function is implemented which always returns false (no
//! time available)
//!
//! @note It's expected that the mutexes implemented are recursive mutexes.
void memfault_lock(void);
void memfault_unlock(void);

#ifdef __cplusplus
}
#endif
