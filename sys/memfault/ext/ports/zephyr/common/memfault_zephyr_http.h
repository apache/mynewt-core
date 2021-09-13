#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Zephyr-port specific http utility for interfacing with http

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif
