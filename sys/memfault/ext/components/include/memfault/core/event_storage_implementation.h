#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! The API an event storage implementation must adhere to. A user of the SDK should never need to
//! include this header

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "memfault/core/event_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

struct MemfaultEventStorageImpl {
  //! Opens a session to begin writing a heartbeat event to storage
  //!
  //! @note To close the session, memfault_events_storage_finish_write() must be called
  //!
  //! @return the free space in storage for the write
  size_t (*begin_write_cb)(void);

  //! Called to append more data to the current event
  //!
  //! @note This function can be called multiple times to make it easy for an event to
  //!   be stored in chunks
  //!
  //! @param bytes Buffer of data to add to the current event
  //! @param num_bytes The total number of bytes to store
  //!
  //! @return true if the write was successful, false otherwise
  bool (*append_data_cb)(const void *bytes, size_t num_bytes);

  //! Called to close a heartbeat event session
  //!
  //! @param rollback If false, the event being stored is committed meaning a future call
  //!  to "g_memfault_event_data_source.has_more_msgs_cb" will return the event. If true,
  //!  the event that was being stored is discarded
  void (*finish_write_cb)(bool rollback);

  //! Returns the _total_ size that can be used by event storage
  size_t (*get_storage_size_cb)(void);
};

#ifdef __cplusplus
}
#endif
