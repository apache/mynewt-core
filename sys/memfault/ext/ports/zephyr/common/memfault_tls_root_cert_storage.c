//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Installs root certificates using Zephyrs default infrastructure

#include "memfault/ports/zephyr/root_cert_storage.h"

#include <net/tls_credentials.h>

#include "memfault/core/compiler.h"
#include "memfault/core/debug_log.h"

#define MEMFAULT_NUM_CERTS_REGISTERED 4

#if !defined(CONFIG_TLS_MAX_CREDENTIALS_NUMBER) || (CONFIG_TLS_MAX_CREDENTIALS_NUMBER < MEMFAULT_NUM_CERTS_REGISTERED)
# pragma message("ERROR: CONFIG_TLS_MAX_CREDENTIALS_NUMBER must be >= "MEMFAULT_EXPAND_AND_QUOTE(MEMFAULT_NUM_CERTS_REGISTERED))
# error "Update CONFIG_TLS_MAX_CREDENTIALS_NUMBER in prj.conf"
#endif

#if !defined(CONFIG_NET_SOCKETS_TLS_MAX_CREDENTIALS) || (CONFIG_NET_SOCKETS_TLS_MAX_CREDENTIALS < MEMFAULT_NUM_CERTS_REGISTERED)
# pragma message("ERROR: CONFIG_NET_SOCKETS_TLS_MAX_CREDENTIALS must be >= "MEMFAULT_EXPAND_AND_QUOTE(MEMFAULT_NUM_CERTS_REGISTERED))
# error "Update CONFIG_NET_SOCKETS_TLS_MAX_CREDENTIALS in prj.conf"
#endif

MEMFAULT_STATIC_ASSERT(
    (kMemfaultRootCert_MaxIndex - (kMemfaultRootCert_Base + 1)) == MEMFAULT_NUM_CERTS_REGISTERED,
    "MEMFAULT_NUM_CERTS_REGISTERED define out of sync");

int memfault_root_cert_storage_add(
    eMemfaultRootCert cert_id, const char *cert, size_t cert_length) {
  return tls_credential_add(cert_id, TLS_CREDENTIAL_CA_CERTIFICATE, cert, cert_length);
}
