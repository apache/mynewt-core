#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Various data providers for the "data_packetizer" which packetizes data collected by the
//! Memfault SDK into payloads that can be sent over the transport used up to the cloud
//!
//! @note A data source must implement three functions which are documented in the function typedefs below
//!
//! @note A weak function implementation of all the provider functions is defined within the
//! memfault data packetizer. This way a user can easily add or remove provider functionality by
//! compiling or not-compiling certain "components" in the SDK
//!
//! @note These APIs are only for use within the SDK itself, a user of the SDK should _never_ need
//! to call them directly

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! Check if there is another message in the data source available for reading
//!
//! @note This function is idempotent and thus should be safe to call multiple times
//!
//! @param total_size On return, populated with total size of the next message
//!  available for reading or 0 if there is no new message available
//!
//! @return true if there is a message which can be read
typedef bool (MemfaultDataSourceHasMoreMessagesCallback)(size_t *next_msg_size);

//! Read the requested bytes for the currently queued up message
//!
//! @param The offset to begin reading at
//! @param buf The buffer to copy the read data into
//! @param buf_len The size of the result buffer
//!
//! @return true if the read was successful, false otherwise (for example if the
//! read goes past the next_msg_size returned from MemfaultDataSourceHasMoreMessagesCallback
typedef bool (MemfaultDataSourceReadMessageCallback)(uint32_t offset, void *buf, size_t buf_len);

//! Delete the currently queued up message being read
//!
//! A subsequent call to the paired MemfaultDataSourceHasMoreMessagesCallback will return
//! a info about a new message or nothing if there are no more messages to read
typedef void (MemfaultDataSourceMarkMessageReadCallback)(void);

typedef struct MemfaultDataSourceImpl {
  MemfaultDataSourceHasMoreMessagesCallback *has_more_msgs_cb;
  MemfaultDataSourceReadMessageCallback *read_msg_cb;
  MemfaultDataSourceMarkMessageReadCallback *mark_msg_read_cb;
} sMemfaultDataSourceImpl;

//! "Coredump" data source provided as part of "panics" component
extern const sMemfaultDataSourceImpl g_memfault_coredump_data_source;

//! Events (i.e "Heartbeat Metrics" & "Reset Reasons") provided as part of "metrics" and "panics"
//! component, respectively
extern const sMemfaultDataSourceImpl g_memfault_event_data_source;

//! Logging data source provided as part of "core" component (memfault/core/log.h)
extern const sMemfaultDataSourceImpl g_memfault_log_data_source;

#ifdef __cplusplus
}
#endif
