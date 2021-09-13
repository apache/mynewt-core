#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#ifdef __cplusplus
extern "C" {
#endif

//! A placeholder for initializing reboot tracking
//!
//! @note A user of the SDK can chose to implement this function and call it during bootup.
//!
//! @note A reference implementation can be found in the NRF52 demo application:
//!  ports/nrf5_sdk/resetreas_reboot_tracking.c
void memfault_platform_reboot_tracking_boot(void);

#ifdef __cplusplus
}
#endif
