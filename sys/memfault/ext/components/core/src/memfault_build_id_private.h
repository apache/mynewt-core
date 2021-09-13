#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Internal file that should never be included by a consumer of the SDK. See
//! "memfault/core/build_info.h" for details on how to leverage the build id.

#include <stddef.h>
#include <stdint.h>

#include "memfault/core/compiler.h"
#include "memfault/version.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// Note: These structures and values are also used in $MEMFAULT_FIRMWARE_SDK/scripts/fw_build_id.py
// Any change here will likely require an update to the script was well!
//

typedef enum {
  //! No Build ID present.
  kMemfaultBuildIdType_None = 1,
  //! Build Id which can be emitted by GCC/LLVM compilers (https://mflt.io/gnu-build-id)
  kMemfaultBuildIdType_GnuBuildIdSha1 = 2,
  //! Build Id Type patched in by $MEMFAULT_FIRMWARE_SDK/scripts/fw_build_id.py
  kMemfaultBuildIdType_MemfaultBuildIdSha1 = 3,
} eMemfaultBuildIdType;

typedef struct {
  uint8_t type; // eMemfaultBuildIdType
  uint8_t len;
  // the length, in bytes, of the build id used when reporting data
  uint8_t short_len;
  uint8_t rsvd;
  const void *storage;
  const sMfltSdkVersion sdk_version;
} sMemfaultBuildIdStorage;

MEMFAULT_STATIC_ASSERT(((offsetof(sMemfaultBuildIdStorage, type) == 0) &&
                        (offsetof(sMemfaultBuildIdStorage, short_len) == 2)),
                       "be sure to update fw_build_id.py!");

#if defined(MEMFAULT_UNITTEST)
//! NB: For unit tests we want to be able to instrument the data in the test
//! so we drop the `const` qualifier
#define MEMFAULT_BUILD_ID_QUALIFIER
#else
#define MEMFAULT_BUILD_ID_QUALIFIER const
#endif

extern MEMFAULT_BUILD_ID_QUALIFIER sMemfaultBuildIdStorage g_memfault_build_id;
extern MEMFAULT_BUILD_ID_QUALIFIER uint8_t g_memfault_sdk_derived_build_id[];

//! The layout of a Note section in an ELF. This is how Build Id information is layed out when
//! using kMemfaultBuildIdType_GnuBuildIdSha1
typedef MEMFAULT_PACKED_STRUCT {
  uint32_t namesz;
  uint32_t descsz;
  uint32_t type;
  char  namedata[];
} sMemfaultElfNoteSection;

#ifdef __cplusplus
}
#endif
