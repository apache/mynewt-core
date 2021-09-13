#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Ports to facilitate the integration of Memfault reboot reason tracking
//! For more details about the integration, see https://mflt.io/reboot-reasons

#include "memfault/core/reboot_tracking.h"

#ifdef __cplusplus
extern "C" {
#endif

//! Reads platform reset reason registers and converts to format suitable for reporting to Memfault
//!
//! @note Reboot reasons are recorded and reported to the Memfault UI and
//! serve as a leading indicator to issues being seen in the field.
//! @note For MCUs where reset reason register information is "sticky" (persists across resets),
//! the platform port will clear the register. This way the next boot will be guaranteed to reflect
//! the correct information for the reboot that just took place.
//! @note By default, ports print the MCU reset information on bootup using the MEMFAULT_LOG
//! infrastructure. This can optionally be disabled by adding -DMEMFAULT_ENABLE_REBOOT_DIAG_DUMP=0
//! to your list of compiler flags
//!
//! @param[out] info Populated with platform specific reset reason info. Notably, reset_reason_reg
//!  is populated with the reset reason register value and reset_reason is populated with a
//!  eMemfaultRebootReason which will be displayed in the Memfault UI
void memfault_reboot_reason_get(sResetBootupInfo *info);

#ifdef __cplusplus
}
#endif
