#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Zephyr specific http utility for interfacing with Memfault HTTP utilities

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//! Installs the root CAs Memfault uses (the certs in "memfault/http/root_certs.h")
//!
//! @note: MUST be called prior to LTE network init (calling NRF's lte_lc_init_and_connect())
//!  for the settings to take effect
//!
//! @return 0 on success, else error code
int memfault_zephyr_port_install_root_certs(void);

//! Sends diagnostic data to the Memfault cloud chunks endpoint
//!
//! @note This function is blocking and will return once posting of chunk data has completed.
//!
//! For more info see https://mflt.io/data-to-cloud
int memfault_zephyr_port_post_data(void);

typedef struct MemfaultOtaInfo {
  // The size, in bytes, of the OTA payload.
  size_t size;
} sMemfaultOtaInfo;

typedef struct {
  //! Caller provided buffer to be used for the duration of the OTA lifecycle
  void *buf;
  size_t buf_len;

  //! Optional: Context for use by the caller
  void *user_ctx;

  //! Called if a new ota update is available
  //! @return true to continue, false to abort the OTA download
  bool (*handle_update_available)(const sMemfaultOtaInfo *info, void *user_ctx);

  //! Invoked as bytes are downloaded for the OTA update
  //!
  //! @return true to continue, false to abort the OTA download
  bool (*handle_data)(void *buf, size_t buf_len, void *user_ctx);

  //! Called once the entire ota payload has been downloaded
  bool (*handle_download_complete)(void *user_ctx);
} sMemfaultOtaUpdateHandler;


//! Handler which can be used to run OTA update using Memfault's Release Mgmt Infra
//! For more details see:
//!  https://mflt.io/release-mgmt
//!
//! @param handler Context with info necessary to perform an OTA. See struct
//!  definition for more details.
//!
//! @note This function is blocking. 'handler' callbacks will be invoked prior to the function
//!   returning.
//!
//! @return
//!   < 0 Error while trying to figure out if an update was available
//!     0 Check completed successfully - No new update available
//!     1 New update is available and handlers were invoked
int memfault_zephyr_port_ota_update(const sMemfaultOtaUpdateHandler *handler);

//! Query Memfault's Release Mgmt Infra for an OTA update
//!
//! @param download_url populated with a string containing the download URL to use
//! if an OTA update is available.
//!
//! @note After use, memfault_zephyr_port_release_download_url() must be called
//!  to free the memory where the download URL is stored.
//!
//! @return
//!   < 0 Error while trying to figure out if an update was available
//!     0 Check completed successfully - No new update available
//!     1 New update is available and download_url has been populated with
//!       the url to use for download
int memfault_zephyr_port_get_download_url(char **download_url);

//! Releases the memory returned from memfault_zephyr_port_get_download_url()
int memfault_zephyr_port_release_download_url(char **download_url);

#ifdef __cplusplus
}
#endif
