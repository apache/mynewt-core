#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! An abstraction to facilitate implementing a debuggable watchdog subsystem using the ideas
//! discussed in https://mflt.io/root-cause-watchdogs

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "memfault/config.h"

//! Starts a software watchdog with a timeout of MEMFAULT_WATCHDOG_SW_TIMEOUT_SECS
//!
//! When the software watchdog expires, a platform specific ISR handler will
//! be invoked. We recommend wiring this handler directly up to the memfault-firmware-sdk by
//! adding the following define to your build system.
//!   -DMEMFAULT_EXC_HANDLER_WATCHDOG=YourIRQ_Handler
//!
//! @return 0 on success, else error code
int memfault_software_watchdog_enable(void);

//! Stops the software watchdog
//!
//! @return 0 on success, else error code
int memfault_software_watchdog_disable(void);

//! A call to this function restarts the software watchdog countdown
//!
//! @return 0 on success, else error code
int memfault_software_watchdog_feed(void);

//! Resets and changes the software watchdog timeout
//!
//! @return 0 on success, else error code
int memfault_software_watchdog_update_timeout(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
