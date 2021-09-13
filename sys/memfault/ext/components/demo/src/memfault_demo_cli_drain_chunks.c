//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! This file contains an example implementation of the pseudocode included in the Memfault Docs
//! https://mflt.io/data-transport-example
//!
//! This CLI command can be used with the Memfault GDB command "memfault install_chunk_handler" to
//! "drain" chunks up to the Memfault cloud directly from gdb.
//!
//! This can be useful when working on integrations and initially getting a transport path in place.
//! (gdb) source $MEMFAULT_SDK/scripts/memfault_gdb.py
//! (gdb) memfault install_chunk_handler -pk <YOUR_PROJECT_KEY>
//! or for more details
//! (gdb) memfault install_chunk_handler --help
//!
//! For more details see https://mflt.io/posting-chunks-with-gdb
//!

#include "memfault/demo/cli.h"

#include "memfault/config.h"
#include "memfault/core/compiler.h"
#include "memfault/core/data_packetizer.h"

// Note: We mark the function as weak so an end user can override this with a real implementation
// and we disable optimizations so the parameters don't get stripped away
MEMFAULT_NO_OPT
MEMFAULT_WEAK
void user_transport_send_chunk_data(MEMFAULT_UNUSED void *chunk_data,
                                    MEMFAULT_UNUSED size_t chunk_data_len) {
}

static bool prv_try_send_memfault_data(void) {
  // buffer to copy chunk data into
  uint8_t buf[MEMFAULT_DEMO_CLI_USER_CHUNK_SIZE];
  size_t buf_len = sizeof(buf);
  bool data_available = memfault_packetizer_get_chunk(buf, &buf_len);
  if (!data_available ) {
    return false; // no more data to send
  }
  // send payload collected to chunks/ endpoint
  user_transport_send_chunk_data(buf, buf_len);
  return true;
}

int memfault_demo_drain_chunk_data(MEMFAULT_UNUSED int argc,
                                   MEMFAULT_UNUSED char *argv[]) {
  while (prv_try_send_memfault_data()) { }
  return 0;
}
