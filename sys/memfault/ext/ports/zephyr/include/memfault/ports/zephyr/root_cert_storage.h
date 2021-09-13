#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! A wrapper around root certificate storage with Zephyr since different modules may use different
//! implementations. For example, the nRF9160 has its own offloaded storage on the modem whereas a
//! external LTE modem may use local Mbed TLS storage on the device.

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  // arbitrarily high base so as not to conflict with id used for other certs in use by the system
  kMemfaultRootCert_Base = 1000,
  kMemfaultRootCert_DigicertRootCa,
  kMemfaultRootCert_DigicertRootG2,
  kMemfaultRootCert_CyberTrustRoot,
  kMemfaultRootCert_AmazonRootCa1,
  // Must be last, used to track number of root certs in use
  kMemfaultRootCert_MaxIndex,
} eMemfaultRootCert;

//! Adds specified certificate to backing store
//!
//! @param cert_id Identifier to be used to reference the certificate
//! @param cert root certificate to add to storage (
//! @param cert_length Length of PEM certificate
//!
//! @return 0 on success or if the cert was already loaded, else error code
int memfault_root_cert_storage_add(eMemfaultRootCert cert_id, const char *cert, size_t cert_length);

#ifdef __cplusplus
}
#endif
