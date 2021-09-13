#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Integrates the Memfault Release Management Infrastructure with the FOTA Client in the nRF
//! Connect SDK

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_MEMFAULT_FOTA
#include "net/fota_download.h"

//! A callback invoked while a FOTA is in progress
//!
//! @note The default implementation will reboot the system after an OTA download has completed.
//!
//! A custom implementation can be provided by setting
//! CONFIG_MEMFAULT_FOTA_DOWNLOAD_CALLBACK_CUSTOM=y and implementing this function
void memfault_fota_download_callback(const struct fota_download_evt *evt);

//! Checks to see if an OTA update is available and downloads the binary if so
//!
//! @return
//!   < 0 Error while trying to figure out if an update was available
//!     0 Check completed successfully - No new update available
//!     1 New update is available and handlers were invoked
int memfault_fota_start(void);

#endif /* CONFIG_MEMFAULT_FOTA */

#ifdef __cplusplus
}
#endif
