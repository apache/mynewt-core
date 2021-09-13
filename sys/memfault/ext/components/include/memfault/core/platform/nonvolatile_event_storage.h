#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Dependencies which must be implemented to persist events collected by the Memfault SDK into
//! non-volatile storage. This can be useful when a device:
//!  - has prolonged periods without connectivity causing many events to get batched up
//!  - is likely to reboot in between connections (i.e. due to low battery, user initiated
//!    resets, etc)

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! Callback passed into the non-volatile storage write() dependency to read an event
typedef bool (MemfaultEventReadCallback)(uint32_t offset, void *buf, size_t buf_len);

typedef struct MemfaultNonVolatileEventStorageImpl {
  //! @return if true, the Memfault SDK will persist events here when
  //! "memfault_event_storage_persist" is called. if false, none of the other
  //! dependencies will be called and events will not be saved in non-volatile storage.
  bool (*enabled)(void);

  //! Check if there is an event ready to be consumed from non-volatile storage
  //!
  //! @note This function is idempotent and thus must be safe to call multiple times
  //!
  //! @param[out] event_length_out The size of the next stored event to read
  //!
  //! @return true if there is at least one event stored
  bool (*has_event)(size_t *event_length_out);

  //! Read the requested bytes for the currently queued up message
  //! @return true if the read was successful, false otherwise
  bool (*read)(uint32_t offset, void *buf, size_t buf_len);

  //! Delete the currently queued up message being read
  //!
  //! @note The next call to "has_event" should return info about the next queued event to be
  //! "read".
  void (*consume)(void);

  //! Write the event provided into storage
  //!
  //! @param reader_callback Helper for reading event to be written.
  //! @param total_size The total size of the event to save
  bool (*write)(MemfaultEventReadCallback reader_callback, size_t total_size);

} sMemfaultNonVolatileEventStorageImpl;

//! By default a weak definition of this structure is provided and the feature is disabled
//!
//! @note May optionally be implemented by a SDK user to save events to non-volatile storage
//!
//! @note It's up to the end user to decide when to flush events since that is use case specific
//! The easiest way to achieve this is to add the following to your port:
//!
//! #include "memfault/core/event_storage.h"
//! void memfault_event_storage_request_persist_callback(
//!     const sMemfaultEventStoragePersistCbStatus *status) {
//!   memfault_event_storage_persist();
//! }
extern const sMemfaultNonVolatileEventStorageImpl g_memfault_platform_nv_event_storage_impl;

#ifdef __cplusplus
}
#endif
