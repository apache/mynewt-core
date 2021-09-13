//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! A RAM-backed storage API for serialized events. This is where events (such as heartbeats and
//! reset trace events) get stored as they wait to be chunked up and sent out over the transport.

#include "memfault/core/event_storage.h"
#include "memfault/core/event_storage_implementation.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "memfault/config.h"
#include "memfault/core/batched_events.h"
#include "memfault/core/compiler.h"
#include "memfault/core/data_packetizer_source.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/math.h"
#include "memfault/core/platform/nonvolatile_event_storage.h"
#include "memfault/core/platform/overrides.h"
#include "memfault/core/platform/system_time.h"
#include "memfault/core/sdk_assert.h"
#include "memfault/util/circular_buffer.h"

//
// Routines which can optionally be implemented.
// For more details see:
//  memfault/core/platform/system_time.h
//  memfault/core/platform/overrides.h
//  memfault/core/platform/event.h
//

MEMFAULT_WEAK bool memfault_platform_time_get_current(MEMFAULT_UNUSED sMemfaultCurrentTime *time) {
  return false;
}

MEMFAULT_WEAK
void memfault_lock(void) { }

MEMFAULT_WEAK
void memfault_unlock(void) { }

MEMFAULT_WEAK
void memfault_event_storage_request_persist_callback(
    MEMFAULT_UNUSED const sMemfaultEventStoragePersistCbStatus *status) { }

static bool prv_nonvolatile_event_storage_enabled(void) {
  return false;
}

MEMFAULT_WEAK
const sMemfaultNonVolatileEventStorageImpl g_memfault_platform_nv_event_storage_impl = {
  .enabled = prv_nonvolatile_event_storage_enabled,
};

typedef struct {
  bool write_in_progress;
  size_t bytes_written;
} sMemfaultEventStorageWriteState;

typedef struct {
  size_t active_event_read_size;
  size_t num_events;
  sMemfaultBatchedEventsHeader event_header;
} sMemfaultEventStorageReadState;

#define MEMFAULT_EVENT_STORAGE_WRITE_IN_PROGRESS 0xffff

typedef MEMFAULT_PACKED_STRUCT {
  uint16_t total_size;
} sMemfaultEventStorageHeader;

static sMfltCircularBuffer s_event_storage;
static sMemfaultEventStorageWriteState s_event_storage_write_state;
static sMemfaultEventStorageReadState s_event_storage_read_state;

static void prv_invoke_request_persist_callback(void) {
  sMemfaultEventStoragePersistCbStatus status;
  memfault_lock();
  {
    status = (sMemfaultEventStoragePersistCbStatus) {
      .volatile_storage = {
        .bytes_used = memfault_circular_buffer_get_read_size(&s_event_storage),
        .bytes_free = memfault_circular_buffer_get_write_size(&s_event_storage),
      },
    };
  }
  memfault_unlock();

  memfault_event_storage_request_persist_callback(&status);
}

static size_t prv_get_total_event_size(sMemfaultEventStorageReadState *state) {
  if (state->num_events == 0) {
    return 0;
  }

  const size_t hdr_overhead_bytes = state->num_events * sizeof(sMemfaultEventStorageHeader);
  return (state->active_event_read_size + state->event_header.length) - hdr_overhead_bytes;
}

//! Walk the ram-backed event storage and determine data to read
//!
//! @return true if computation was successful, false otherwise
static void prv_compute_read_state(sMemfaultEventStorageReadState *state) {
  *state = (sMemfaultEventStorageReadState) { 0 };
  while (1) {
    sMemfaultEventStorageHeader hdr = { 0 };
    const bool success = memfault_circular_buffer_read(
        &s_event_storage, state->active_event_read_size, &hdr, sizeof(hdr));
    if (!success || hdr.total_size == MEMFAULT_EVENT_STORAGE_WRITE_IN_PROGRESS) {
      break;
    }

    state->num_events++;
    state->active_event_read_size += hdr.total_size;

#if (MEMFAULT_EVENT_STORAGE_READ_BATCHING_ENABLED == 0)
    // if batching is disabled, only one event will be read at a time
    break;
#else
    if ((state->num_events > 1) &&
        (prv_get_total_event_size(state) > MEMFAULT_EVENT_STORAGE_READ_BATCHING_MAX_BYTES)) {
      // more bytes than desired, so don't count this event
      state->num_events--;
      state->active_event_read_size -= hdr.total_size;
      break;
    }
#endif /* MEMFAULT_EVENT_STORAGE_READ_BATCHING_ENABLED */
  }

#if (MEMFAULT_EVENT_STORAGE_READ_BATCHING_ENABLED != 0)
  memfault_batched_events_build_header(state->num_events, &state->event_header);
#endif
}

static bool prv_has_data_ram(size_t *total_size) {
  // Check to see if a read is already in progress and return that size if true
  size_t curr_read_size;
  memfault_lock();
  {
    curr_read_size = prv_get_total_event_size(&s_event_storage_read_state);
  }
  memfault_unlock();

  if (curr_read_size != 0) {
    *total_size = curr_read_size;
    return ((*total_size) != 0);
  }

  // see if there are any events to read
  sMemfaultEventStorageReadState read_state;
  memfault_lock();
  {
    prv_compute_read_state(&read_state);
    s_event_storage_read_state = read_state;
  }
  memfault_unlock();


  *total_size = prv_get_total_event_size(&s_event_storage_read_state);
  return ((*total_size) != 0);
}

static bool prv_event_storage_read_ram(uint32_t offset, void *buf, size_t buf_len) {
  const size_t total_event_size = prv_get_total_event_size(&s_event_storage_read_state);
  if ((offset + buf_len) > total_event_size) {
    return false;
  }

  // header_length != 0 when we encode multiple events in a single read so
  // first check to see if we need to copy any of that
  uint8_t *bufp = (uint8_t *)buf;
  if (offset < s_event_storage_read_state.event_header.length) {
    const size_t bytes_to_copy = MEMFAULT_MIN(
        buf_len, s_event_storage_read_state.event_header.length - offset);
    memcpy(bufp, &s_event_storage_read_state.event_header.data[offset], bytes_to_copy);
    buf_len -= bytes_to_copy;

    offset = 0;
    bufp += bytes_to_copy;
  } else {
    offset -= s_event_storage_read_state.event_header.length;
  }

  uint32_t curr_offset = 0;
  uint32_t read_offset = 0;

  while (buf_len > 0) {
    sMemfaultEventStorageHeader hdr = { 0 };
    const bool success = memfault_circular_buffer_read(
        &s_event_storage, read_offset, &hdr, sizeof(hdr));
    if (!success) {
      // not possible to get here unless there is corruption
      return false;
    }

    read_offset += sizeof(hdr);
    const size_t event_size = hdr.total_size - sizeof(hdr);

    if ((curr_offset + event_size) < offset) {
      // we haven't reached the offset we were trying to read from
      curr_offset += event_size;
      read_offset += event_size;
      continue;
    }

    // offset within the event to start reading at
    const size_t evt_start_offset = offset - curr_offset;

    const size_t bytes_to_read = MEMFAULT_MIN(event_size - evt_start_offset, buf_len);
    if (!memfault_circular_buffer_read(&s_event_storage, read_offset + evt_start_offset,
                                       bufp, bytes_to_read)) {
      // not possible to get here unless there is corruption
      return false;
    }

    bufp += bytes_to_read;
    curr_offset += event_size;
    read_offset += event_size;
    buf_len -= bytes_to_read;
    offset += bytes_to_read;
  }

  return true;
}

static void prv_event_storage_mark_event_read_ram(void) {
  if (s_event_storage_read_state.active_event_read_size == 0) {
    // no active event to clear
    return;
  }

  memfault_lock();
  {
    memfault_circular_buffer_consume(
        &s_event_storage, s_event_storage_read_state.active_event_read_size);
    s_event_storage_read_state = (sMemfaultEventStorageReadState) { 0 };
  }
  memfault_unlock();
}

// "begin" to write event data & return the space available
static size_t prv_event_storage_storage_begin_write(void) {
  if (s_event_storage_write_state.write_in_progress) {
    return 0;
  }

  const sMemfaultEventStorageHeader hdr = {
    .total_size = MEMFAULT_EVENT_STORAGE_WRITE_IN_PROGRESS,
  };
  bool success;
  memfault_lock();
  {
    success = memfault_circular_buffer_write(&s_event_storage, &hdr, sizeof(hdr));
  }
  memfault_unlock();
  if (!success) {
    return 0;
  }

  s_event_storage_write_state = (sMemfaultEventStorageWriteState) {
    .write_in_progress = true,
    .bytes_written = sizeof(hdr),
  };

  return memfault_circular_buffer_get_write_size(&s_event_storage);
}

static bool prv_event_storage_storage_append_data(const void *bytes, size_t num_bytes) {
  bool success;

  memfault_lock();
  {
    success = memfault_circular_buffer_write(&s_event_storage, bytes, num_bytes);
  }
  memfault_unlock();
  if (success) {
    s_event_storage_write_state.bytes_written += num_bytes;
  }
  return success;
}

static void prv_event_storage_storage_finish_write(bool rollback) {
  if (!s_event_storage_write_state.write_in_progress) {
    return;
  }

  memfault_lock();
  {
    if (rollback) {
      memfault_circular_buffer_consume_from_end(&s_event_storage,
                                                s_event_storage_write_state.bytes_written);
    } else {
      const sMemfaultEventStorageHeader hdr = {
        .total_size = (uint16_t)s_event_storage_write_state.bytes_written,
      };
      memfault_circular_buffer_write_at_offset(&s_event_storage,
                                               s_event_storage_write_state.bytes_written,
                                               &hdr, sizeof(hdr));
    }
  }
  memfault_unlock();

  // reset the write state
  s_event_storage_write_state = (sMemfaultEventStorageWriteState) { 0 };
  if (!rollback) {
    prv_invoke_request_persist_callback();
  }
}

static size_t prv_get_size_cb(void) {
  return memfault_circular_buffer_get_read_size(&s_event_storage) +
      memfault_circular_buffer_get_write_size(&s_event_storage);
}

const sMemfaultEventStorageImpl *memfault_events_storage_boot(void *buf, size_t buf_len) {
  memfault_circular_buffer_init(&s_event_storage, buf, buf_len);

  s_event_storage_write_state = (sMemfaultEventStorageWriteState) { 0 };
  s_event_storage_read_state = (sMemfaultEventStorageReadState) { 0 };

  static const sMemfaultEventStorageImpl s_event_storage_impl = {
    .begin_write_cb = &prv_event_storage_storage_begin_write,
    .append_data_cb = &prv_event_storage_storage_append_data,
    .finish_write_cb = &prv_event_storage_storage_finish_write,
    .get_storage_size_cb = &prv_get_size_cb,
  };
  return &s_event_storage_impl;
}

static bool prv_save_event_to_persistent_storage(void) {
  size_t total_size;
  if (!prv_has_data_ram(&total_size)) {
    return false;
  }

  const bool success = g_memfault_platform_nv_event_storage_impl.write(
      prv_event_storage_read_ram, total_size);
  if (success) {
    prv_event_storage_mark_event_read_ram();
  }
  return success;
}

static bool prv_nv_event_storage_enabled(void) {
  static bool s_nv_event_storage_enabled = false;

  MEMFAULT_SDK_ASSERT(g_memfault_platform_nv_event_storage_impl.enabled != NULL);
  const bool enabled = g_memfault_platform_nv_event_storage_impl.enabled();
  if (s_nv_event_storage_enabled && !enabled) {
    // This shouldn't happen and is indicative of a failure in nv storage. Let's reset the read
    // state in case we were in the middle of a read() trying to copy data into nv storage.
    s_event_storage_read_state = (sMemfaultEventStorageReadState) { 0 };
  }
  if (enabled) {
    // if nonvolatile storage is enabled, it is a configuration error if all the
    // required dependencies are not implemented!
    MEMFAULT_SDK_ASSERT(
        (g_memfault_platform_nv_event_storage_impl.has_event != NULL) &&
        (g_memfault_platform_nv_event_storage_impl.read != NULL) &&
        (g_memfault_platform_nv_event_storage_impl.consume != NULL) &&
        (g_memfault_platform_nv_event_storage_impl.write != NULL));
  }

  s_nv_event_storage_enabled = enabled;
  return s_nv_event_storage_enabled;
}

int memfault_event_storage_persist(void) {
  if (!prv_nv_event_storage_enabled()) {
    return 0;
  }

  int events_saved = 0;
  while (prv_save_event_to_persistent_storage()) {
    events_saved++;
  }

  return events_saved;
}

#if MEMFAULT_EVENT_STORAGE_NV_SUPPORT_ENABLED
static void prv_nv_event_storage_mark_read_cb(void) {
  g_memfault_platform_nv_event_storage_impl.consume();

  size_t total_size;
  if (!prv_has_data_ram(&total_size)) {
    return;
  }

  prv_invoke_request_persist_callback();
}
#endif /* MEMFAULT_EVENT_STORAGE_NV_SUPPORT_ENABLED */

static const sMemfaultDataSourceImpl *prv_get_active_event_storage_source(void) {
  static const sMemfaultDataSourceImpl s_memfault_ram_event_storage  = {
    .has_more_msgs_cb = prv_has_data_ram,
    .read_msg_cb = prv_event_storage_read_ram,
    .mark_msg_read_cb = prv_event_storage_mark_event_read_ram,
  };

#if MEMFAULT_EVENT_STORAGE_NV_SUPPORT_ENABLED
  static sMemfaultDataSourceImpl s_memfault_nv_event_storage = { 0 };
  s_memfault_nv_event_storage = (sMemfaultDataSourceImpl) {
    .has_more_msgs_cb = g_memfault_platform_nv_event_storage_impl.has_event,
    .read_msg_cb = g_memfault_platform_nv_event_storage_impl.read,
    .mark_msg_read_cb = prv_nv_event_storage_mark_read_cb,
  };

  return prv_nv_event_storage_enabled() ? &s_memfault_nv_event_storage :
                                          &s_memfault_ram_event_storage;
#else
  return &s_memfault_ram_event_storage;
#endif /* MEMFAULT_EVENT_STORAGE_NV_SUPPORT_ENABLED */
}

static bool prv_has_event(size_t *event_size) {
  const sMemfaultDataSourceImpl *impl = prv_get_active_event_storage_source();
  return impl->has_more_msgs_cb(event_size);
}

static bool prv_event_storage_read(uint32_t offset, void *buf, size_t buf_len) {
  const sMemfaultDataSourceImpl *impl = prv_get_active_event_storage_source();
  return impl->read_msg_cb(offset, buf, buf_len);
}

static void prv_event_storage_mark_event_read(void) {
  const sMemfaultDataSourceImpl *impl = prv_get_active_event_storage_source();
  impl->mark_msg_read_cb();
}

//! Expose a data source for use by the Memfault Packetizer
const sMemfaultDataSourceImpl g_memfault_event_data_source  = {
  .has_more_msgs_cb = prv_has_event,
  .read_msg_cb = prv_event_storage_read,
  .mark_msg_read_cb = prv_event_storage_mark_event_read,
};

// These getters provide the information that user doesn't have. The user knows the total size
// of the event storage because they supply it but they need help to get the free/used stats.
size_t memfault_event_storage_bytes_used(void) {
  size_t bytes_used;

  memfault_lock();
  {
    bytes_used = memfault_circular_buffer_get_read_size(&s_event_storage);
  }
  memfault_unlock();

  return bytes_used;
}

size_t memfault_event_storage_bytes_free(void) {
  size_t bytes_free;

  memfault_lock();
  {
    bytes_free = memfault_circular_buffer_get_write_size(&s_event_storage);
  }
  memfault_unlock();

  return bytes_free;
}
