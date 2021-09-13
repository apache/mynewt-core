//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! CLI command that dumps the coredump saved out over the console in such a way that the output
//! can be copy & pasted to a terminal and posted to the Memfault cloud Service

#include "memfault/demo/cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memfault/core/compiler.h"
#include "memfault/core/data_packetizer.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/math.h"
#include "memfault/demo/util.h"

//
// Weak function implementations for when the "http" component is not enabled. This way we can
// still dump a CLI command that shows how to post a chunk using curl.
//

MEMFAULT_WEAK
const char *memfault_demo_get_chunks_url(void) {
  return "https://chunks.memfault.com/api/v0/chunks/DEMOSERIAL";
}

MEMFAULT_WEAK
const char *memfault_demo_get_api_project_key(void) {
  return "<YOUR PROJECT KEY>";
}

static void prv_write_curl_epilogue(void) {
  MEMFAULT_LOG_RAW("| xxd -p -r | curl -X POST %s\\", memfault_demo_get_chunks_url());
  MEMFAULT_LOG_RAW(" -H 'Memfault-Project-Key:%s'\\", memfault_demo_get_api_project_key());
  MEMFAULT_LOG_RAW(" -H 'Content-Type:application/octet-stream' --data-binary @- -i");
  MEMFAULT_LOG_RAW("\nprint_chunk done");
}

typedef struct MemfaultPrintImpl {
  const char *prologue;
  const char *line_end;
  void (*write_epilogue)(void);
} sMemfaultPrintImpl;

static void prv_write_chunk_data(const sMemfaultPrintImpl *print_impl, uint8_t *packet_buffer,
                                 size_t packet_size) {
  char hex_buffer[MEMFAULT_CLI_LOG_BUFFER_MAX_SIZE_BYTES];
  for (uint32_t j = 0; j < packet_size; ++j) {
    sprintf(&hex_buffer[j * 2], "%02x", packet_buffer[j]);
  }
  strncpy(&hex_buffer[packet_size * 2], print_impl->line_end,
          MEMFAULT_CLI_LOG_BUFFER_MAX_SIZE_BYTES - (packet_size * 2));
  MEMFAULT_LOG_RAW("%s", hex_buffer);
}

static int prv_send_memfault_data_multi_packet_chunk(
    const sMemfaultPrintImpl *print_impl, void *packet_buffer, size_t packet_buffer_max_size) {
  const sPacketizerConfig cfg = {
    // Enable multi packet chunking. This means a chunk may span multiple calls to
    // memfault_packetizer_get_next().
    .enable_multi_packet_chunk = true,
  };

  sPacketizerMetadata metadata;
  bool data_available = memfault_packetizer_begin(&cfg, &metadata);
  if (!data_available) {
    return 0;
  }

  while (1) {
    size_t read_size = packet_buffer_max_size;
    eMemfaultPacketizerStatus packetizer_status =
        memfault_packetizer_get_next(packet_buffer, &read_size);
    if (packetizer_status == kMemfaultPacketizerStatus_NoMoreData) {
      // We know data is available from the memfault_packetizer_begin() call above
      // so _NoMoreData is an unexpected result
      MEMFAULT_LOG_ERROR("Unexpected packetizer status: %d", (int)packetizer_status);
      return -1;
    }

    prv_write_chunk_data(print_impl, packet_buffer, read_size);
    if (packetizer_status == kMemfaultPacketizerStatus_EndOfChunk) {
      break;
    }
  }
  return 0;
}

static int prv_send_memfault_data_single_packet_chunk(
    const sMemfaultPrintImpl *print_impl, void *packet_buffer, size_t packet_buffer_size) {
  bool data_available = memfault_packetizer_get_chunk(packet_buffer, &packet_buffer_size);
  if (!data_available) {
    return 0;
  }

  prv_write_chunk_data(print_impl, packet_buffer, packet_buffer_size);
  return 0;
}

int memfault_demo_cli_cmd_print_chunk(int argc, char *argv[]) {
  // by default, we will use curl
  bool use_curl = (argc <= 1) || (strcmp(argv[1], "curl") == 0);
  bool use_hex = !use_curl && (strcmp(argv[1], "hex") == 0);
  if (!use_curl && !use_hex) {
    MEMFAULT_LOG_ERROR("Usage: \"print_chunk\" or \"print_chunk <curl|hex> <chunk_size>\"");
    return -1;
  }

  // by default, we will dump the entire message in a single chunk
  size_t chunk_size = (argc <= 2) ? 0 : (size_t)atoi(argv[2]);

  char packet_buffer[MEMFAULT_CLI_LOG_BUFFER_MAX_SIZE_BYTES / 2];
  if (chunk_size > sizeof(packet_buffer)) {
    MEMFAULT_LOG_ERROR("Usage: chunk_size must be <= %d bytes", (int)sizeof(packet_buffer));
    return -1;
  }

  bool more_data = memfault_packetizer_data_available();
  if (!more_data) { // there are no more chunks to send
    MEMFAULT_LOG_INFO("All data has been sent!");
    return 0;
  }

  const sMemfaultPrintImpl print_impl = {
    .prologue = use_curl ? "echo \\" : NULL,
    .line_end = use_curl ? "\\" : "",
    .write_epilogue = use_curl ? prv_write_curl_epilogue: NULL,
  };

  if (print_impl.prologue) {
    MEMFAULT_LOG_RAW("%s", print_impl.prologue);
  }

  const size_t max_read_size = sizeof(packet_buffer) - strlen(print_impl.line_end) - 1;

  int rv = (chunk_size == 0) ?
      prv_send_memfault_data_multi_packet_chunk(&print_impl, packet_buffer, max_read_size) :
      prv_send_memfault_data_single_packet_chunk(&print_impl, packet_buffer, max_read_size);

  if (print_impl.write_epilogue) {
    print_impl.write_epilogue();
  }
  return rv;
}
