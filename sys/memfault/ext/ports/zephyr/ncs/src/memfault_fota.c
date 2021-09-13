//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details

#include "memfault/nrfconnect_port/http.h"
#include "memfault/nrfconnect_port/fota.h"

#include "memfault/core/debug_log.h"
#include "memfault/core/math.h"
#include "memfault/core/platform/core.h"
#include "memfault/panics/assert.h"
#include "memfault/ports/zephyr/root_cert_storage.h"

#include "net/download_client.h"
#include "net/fota_download.h"

#include <shell/shell.h>

//! Note: A small patch is needed to nrf in order to enable
//! as of the latest SDK release (nRF Connect SDK v1.4.x)
//! See https://mflt.io/nrf-fota for more details.
#if CONFIG_DOWNLOAD_CLIENT_MAX_FILENAME_SIZE < 400
#warning "CONFIG_DOWNLOAD_CLIENT_MAX_FILENAME_SIZE must be >= 400"

#if CONFIG_DOWNLOAD_CLIENT_STACK_SIZE < 1600
#warning "CONFIG_DOWNLOAD_CLIENT_STACK_SIZE must be >= 1600"
#endif

#error "DOWNLOAD_CLIENT_MAX_FILENAME_SIZE range may need to be extended in nrf/subsys/net/lib/download_client/Kconfig"
#endif

#if !CONFIG_MEMFAULT_FOTA_DOWNLOAD_CALLBACK_CUSTOM
void memfault_fota_download_callback(const struct fota_download_evt *evt) {
  switch (evt->id) {
    case FOTA_DOWNLOAD_EVT_FINISHED:
      MEMFAULT_LOG_INFO("OTA Complete, resetting to install update!");
      memfault_platform_reboot();
      break;
    default:
      break;
  }
}
#endif

int memfault_fota_start(void) {
  char *download_url = NULL;
  int rv = memfault_zephyr_port_get_download_url(&download_url);
  if (rv <= 0) {
    return rv;
  }

  MEMFAULT_ASSERT(download_url != NULL);

  MEMFAULT_LOG_INFO("FOTA Update Available. Starting Download!");
  rv = fota_download_init(&memfault_fota_download_callback);
  if (rv != 0) {
    MEMFAULT_LOG_ERROR("FOTA init failed, rv=%d", rv);
    goto cleanup;
  }

  // Note: The nordic FOTA API only supports passing one root CA today. So we cycle through the
  // list of required Root CAs in use by Memfault to find the appropriate one
  const int certs[] = { kMemfaultRootCert_CyberTrustRoot, kMemfaultRootCert_DigicertRootCa,
                        kMemfaultRootCert_DigicertRootG2, kMemfaultRootCert_AmazonRootCa1 };
  for (size_t i = 0; i < MEMFAULT_ARRAY_SIZE(certs); i++) {
    rv = fota_download_start(download_url, download_url, certs[i], NULL, 0);
    if ((rv != 0) && (errno == EOPNOTSUPP)) {
      // We hit an error that was not due to a TLS handshake failure
      continue;
    }
    break;
  }
  if (rv != 0) {
    MEMFAULT_LOG_ERROR("FOTA start failed, rv=%d", rv);
    return rv;
  }

  MEMFAULT_LOG_INFO("FOTA In Progress");
  rv = 1;
cleanup:
  memfault_zephyr_port_release_download_url(&download_url);
  return rv;
}
