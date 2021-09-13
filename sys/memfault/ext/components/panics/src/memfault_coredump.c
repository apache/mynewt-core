//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Logic for saving a coredump to backing storage and reading it out

#include "memfault/panics/coredump.h"
#include "memfault/panics/coredump_impl.h"

#include <string.h>
#include <stdbool.h>

#include "memfault/core/build_info.h"
#include "memfault/core/compiler.h"
#include "memfault/core/data_packetizer_source.h"
#include "memfault/core/math.h"
#include "memfault/core/platform/device_info.h"
#include "memfault/panics/platform/coredump.h"

#define MEMFAULT_COREDUMP_MAGIC 0x45524f43

//! Version 2
//!  - If there is not enough storage space for memory regions,
//!    coredumps will now be truncated instead of failing completely
//!  - Added sMfltCoredumpFooter to end of coredump. In this region
//!    we track whether or not a coredump was truncated.
#define MEMFAULT_COREDUMP_VERSION 2

typedef MEMFAULT_PACKED_STRUCT MfltCoredumpHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t total_size;
  uint8_t data[];
} sMfltCoredumpHeader;

#define MEMFAULT_COREDUMP_FOOTER_MAGIC 0x504d5544

typedef enum MfltCoredumpFooterFlags  {
  kMfltCoredumpBlockType_SaveTruncated = 0,
} eMfltCoredumpFooterFlags;

typedef MEMFAULT_PACKED_STRUCT MfltCoredumpFooter {
  uint32_t magic;
  uint32_t flags;
  // reserving for future footer additions such as a CRC over the contents saved
  uint32_t rsvd[2];
} sMfltCoredumpFooter;

typedef MEMFAULT_PACKED_STRUCT MfltCoredumpBlock {
  eMfltCoredumpBlockType block_type:8;
  uint8_t rsvd[3];
  uint32_t address;
  uint32_t length;
} sMfltCoredumpBlock;

typedef MEMFAULT_PACKED_STRUCT MfltTraceReasonBlock {
  uint32_t reason;
} sMfltTraceReasonBlock;

// Using ELF machine enum values which is a half word:
//  https://refspecs.linuxfoundation.org/elf/gabi4%2B/ch4.eheader.html
//
// NB: We use the upper 16 bits of the MachineType TLV pair in the coredump to
// encode additional metadata about the architecture being targeted

#define MEMFAULT_MACHINE_TYPE_SUBTYPE_OFFSET 16

#define MEMFAULT_MACHINE_TYPE_XTENSA 94

#define MEMFAULT_MACHINE_TYPE_XTENSA_LX106 \
  ((1 << MEMFAULT_MACHINE_TYPE_SUBTYPE_OFFSET) | MEMFAULT_MACHINE_TYPE_XTENSA)

typedef enum MfltCoredumpMachineType  {
  kMfltCoredumpMachineType_None = 0,
  kMfltCoredumpMachineType_ARM = 40,
  kMfltCoredumpMachineType_Aarch64 = 183,
  kMfltCoredumpMachineType_Xtensa = MEMFAULT_MACHINE_TYPE_XTENSA,
  kMfltCoredumpMachineType_XtensaLx106 = MEMFAULT_MACHINE_TYPE_XTENSA_LX106
} eMfltCoredumpMachineType;

typedef MEMFAULT_PACKED_STRUCT MfltMachineTypeBlock {
  uint32_t machine_type;
} sMfltMachineTypeBlock;

typedef struct {
  // the space available for saving a coredump
  uint32_t storage_size;
  // the offset within storage currently being written to
  uint32_t offset;
  // set to true when no writes should be performed and only the total size of the write should be
  // computed
  bool compute_size_only;
  // set to true if writing a block was truncated
  bool truncated;
  // set to true if a call to "memfault_platform_coredump_storage_write" failed
  bool write_error;
} sMfltCoredumpWriteCtx;

// Checks to see if the block is a cached region and applies
// required fixups to allow the coredump to properly record
// the original cached address and its associated data. Will
// succeed if not a cached block or is a valid cached block.
// Callers should ignore the region if failure is returned
// because the block is not valid.
static bool prv_fixup_if_cached_block(sMfltCoredumpRegion *region, uint32_t *cached_address) {

  if (region->type ==  kMfltCoredumpRegionType_CachedMemory) {
    const sMfltCachedBlock *cached_blk = region->region_start;
    if (!cached_blk->valid_cache) {
      // Ignore this block.
      return false;
    }

    // This is where we want Memfault to indicate the data came from.
    *cached_address = cached_blk->cached_address;

    // The cached block is just regular memory.
    region->type = kMfltCoredumpRegionType_Memory;

    // Remove our header from the size and region_start
    // is where we cached the <cached_address>'s data.
    region->region_size = cached_blk->blk_size;
    region->region_start = cached_blk->blk; // Must be last operation!
  }

  // Success, untouched or fixed up.
  return true;
}

static bool prv_platform_coredump_write(const void *data, size_t len, sMfltCoredumpWriteCtx *write_ctx) {
  // if we are just computing the size needed, don't write any data but keep
  // a count of how many bytes would be written.
  if (!write_ctx->compute_size_only &&
      !memfault_platform_coredump_storage_write(write_ctx->offset, data, len)) {
    write_ctx->write_error = true;
    return false;
  }

  write_ctx->offset += len;
  return true;
}

static bool prv_write_block_with_address(
    eMfltCoredumpBlockType block_type, const void *block_payload, size_t block_payload_size,
    uint32_t address, sMfltCoredumpWriteCtx *write_ctx, bool word_aligned_reads_only) {
  // nothing to write, ignore the request
  if (block_payload_size == 0 || (block_payload == NULL)) {
    return true;
  }

  const size_t total_length = sizeof(sMfltCoredumpBlock) + block_payload_size;
  const size_t storage_bytes_free =
      write_ctx->storage_size > write_ctx->offset ?  write_ctx->storage_size - write_ctx->offset : 0;

  if (!write_ctx->compute_size_only && storage_bytes_free < total_length) {
    // We are trying to write a new block in the coredump and there is not enough
    // space. Let's see if we can truncate the block to fit in the space that is left
    write_ctx->truncated = true;

    if (storage_bytes_free < sizeof(sMfltCoredumpBlock)) {
      return false;
    }

    block_payload_size = MEMFAULT_FLOOR(storage_bytes_free - sizeof(sMfltCoredumpBlock), 4);
    if (block_payload_size == 0) {
      return false;
    }
  }

  const sMfltCoredumpBlock blk = {
    .block_type = block_type,
    .address = address,
    .length = block_payload_size,
  };

  if (!prv_platform_coredump_write(&blk, sizeof(blk), write_ctx)) {
    return false;
  }

  if (!word_aligned_reads_only || ((block_payload_size % 4) != 0)) {
    // no requirements on how the 'address' is read so whatever the user implementation does is fine
    return prv_platform_coredump_write(block_payload, block_payload_size, write_ctx);
  }

  // We have a region that needs to be read 32 bits at a time.
  //
  // Typically these are very small regions such as a memory mapped register address
  const uint32_t *word_data = block_payload;
  for (uint32_t i = 0; i < block_payload_size / 4; i++) {
    const uint32_t data = word_data[i];
    if (!prv_platform_coredump_write(&data, sizeof(data), write_ctx)) {
      return false;
    }
  }

  return !write_ctx->truncated;
}

static bool prv_write_non_memory_block(eMfltCoredumpBlockType block_type,
                                       const void *block_payload, size_t block_payload_size,
                                       sMfltCoredumpWriteCtx *ctx) {
  const bool word_aligned_reads_only = false;
  return prv_write_block_with_address(block_type, block_payload, block_payload_size,
                                      0, ctx, word_aligned_reads_only);
}

static eMfltCoredumpBlockType prv_region_type_to_storage_type(eMfltCoredumpRegionType type) {
  switch (type) {
    case kMfltCoredumpRegionType_ArmV6orV7MpuUnrolled:
      return kMfltCoredumpRegionType_ArmV6orV7Mpu;
    case kMfltCoredumpRegionType_ImageIdentifier:
    case kMfltCoredumpRegionType_Memory:
    case kMfltCoredumpRegionType_MemoryWordAccessOnly:
    case kMfltCoredumpRegionType_CachedMemory:
    default:
      return kMfltCoredumpBlockType_MemoryRegion;
  }
}

static eMfltCoredumpMachineType prv_get_machine_type(void) {
#if defined(MEMFAULT_UNITTEST)
  return kMfltCoredumpMachineType_None;
#else
#  if MEMFAULT_COMPILER_ARM
  return kMfltCoredumpMachineType_ARM;
#  elif defined(__aarch64__)
  return kMfltCoredumpMachineType_Aarch64;
#  elif defined(__XTENSA__)
  # if defined(__XTENSA_WINDOWED_ABI__)
    return kMfltCoredumpMachineType_Xtensa;
  # else
    return kMfltCoredumpMachineType_XtensaLx106;
  # endif
# else
#    error "Coredumps are not supported for target architecture"
#  endif
#endif
}

static bool prv_write_device_info_blocks(sMfltCoredumpWriteCtx *ctx) {
  struct MemfaultDeviceInfo info;
  memfault_platform_get_device_info(&info);

  sMemfaultBuildInfo build_info;
  if (memfault_build_info_read(&build_info)) {
    if (!prv_write_non_memory_block(kMfltCoredumpRegionType_BuildId,
                                    build_info.build_id, sizeof(build_info.build_id), ctx)) {
      return false;
    }
  }

  if (info.device_serial) {
    if (!prv_write_non_memory_block(kMfltCoredumpRegionType_DeviceSerial,
                                    info.device_serial, strlen(info.device_serial), ctx)) {
      return false;
    }
  }

  if (info.software_version) {
    if (!prv_write_non_memory_block(kMfltCoredumpRegionType_SoftwareVersion,
                                    info.software_version, strlen(info.software_version), ctx)) {
      return false;
    }
  }

  if (info.software_type) {
    if (!prv_write_non_memory_block(kMfltCoredumpRegionType_SoftwareType,
                                       info.software_type, strlen(info.software_type), ctx)) {
      return false;
    }
  }

  if (info.hardware_version) {
    if (!prv_write_non_memory_block(kMfltCoredumpRegionType_HardwareVersion,
                                       info.hardware_version, strlen(info.hardware_version), ctx)) {
      return false;
    }
  }

  eMfltCoredumpMachineType machine_type = prv_get_machine_type();
  const sMfltMachineTypeBlock machine_block = {
    .machine_type = (uint32_t)machine_type,
  };
  return prv_write_non_memory_block(kMfltCoredumpRegionType_MachineType,
                                    &machine_block, sizeof(machine_block), ctx);
}

static bool prv_write_coredump_header(size_t total_coredump_size, sMfltCoredumpWriteCtx *ctx) {
  sMfltCoredumpHeader hdr = (sMfltCoredumpHeader) {
    .magic = MEMFAULT_COREDUMP_MAGIC,
    .version = MEMFAULT_COREDUMP_VERSION,
    .total_size = total_coredump_size,
  };
  return prv_platform_coredump_write(&hdr, sizeof(hdr), ctx);
}

static bool prv_write_trace_reason(sMfltCoredumpWriteCtx *ctx, uint32_t trace_reason) {
  sMfltTraceReasonBlock trace_info = {
    .reason = trace_reason,
  };

  return prv_write_non_memory_block(kMfltCoredumpRegionType_TraceReason,
                                    &trace_info, sizeof(trace_info), ctx);
}

// When copying out some regions (for example, memory or register banks)
// we want to make sure we can do word-aligned accesses.
static void prv_insert_padding_if_necessary(sMfltCoredumpWriteCtx *write_ctx) {
  #define MEMFAULT_WORD_SIZE 4
  const size_t remainder = write_ctx->offset % MEMFAULT_WORD_SIZE;
  if (remainder == 0) {
    return;
  }

  #define MEMFAULT_MAX_PADDING_BYTES_NEEDED (MEMFAULT_WORD_SIZE - 1)
  uint8_t pad_bytes[MEMFAULT_MAX_PADDING_BYTES_NEEDED];

  size_t padding_needed = MEMFAULT_WORD_SIZE - remainder;
  memset(pad_bytes, 0x0, padding_needed);

  prv_write_non_memory_block(kMfltCoredumpRegionType_PaddingRegion,
                             &pad_bytes, padding_needed, write_ctx);
}

//! Callback that will be called to write coredump data.
typedef bool(*MfltCoredumpReadCb)(uint32_t offset, void *data, size_t read_len);


static bool prv_get_info_and_header(sMfltCoredumpHeader *hdr_out,
                                    sMfltCoredumpStorageInfo *info_out,
                                    MfltCoredumpReadCb coredump_read_cb) {
  sMfltCoredumpStorageInfo info = { 0 };
  memfault_platform_coredump_storage_get_info(&info);
  if (info.size == 0) {
    return false; // no space for core files!
  }

  if (!coredump_read_cb(0, hdr_out, sizeof(*hdr_out))) {
    // NB: This path is sometimes _expected_. For situations where
    // memfault_platform_coredump_storage_clear() is an asynchronous operation a caller may return
    // false for from memfault_coredump_read() to prevent any access to the coredump storage area.
    return false;
  }

  if (info_out) {
    *info_out = info;
  }
  return true;
}

static bool prv_coredump_get_header(sMfltCoredumpHeader *hdr_out,
                                    MfltCoredumpReadCb coredump_read_cb) {
  return prv_get_info_and_header(hdr_out, NULL, coredump_read_cb);
}

static bool prv_coredump_header_is_valid(const sMfltCoredumpHeader *hdr) {
  return (hdr && hdr->magic == MEMFAULT_COREDUMP_MAGIC);
}

static bool prv_write_regions(sMfltCoredumpWriteCtx *write_ctx, const sMfltCoredumpRegion *regions,
                              size_t num_regions) {
  for (size_t i = 0; i < num_regions; i++) {
    prv_insert_padding_if_necessary(write_ctx);

    // Just in case *regions is some how in r/o memory make a non-const copy
    // and work with that from here on.
    sMfltCoredumpRegion region_copy = regions[i];

    uint32_t address = (uint32_t)(uintptr_t)region_copy.region_start;
    if (!prv_fixup_if_cached_block(&region_copy, &address)) {
      // We must skip invalid cached blocks.
      continue;
    }

    const bool word_aligned_reads_only =
        (region_copy.type == kMfltCoredumpRegionType_MemoryWordAccessOnly);

    if (!prv_write_block_with_address(prv_region_type_to_storage_type(region_copy.type),
                                      region_copy.region_start, region_copy.region_size,
                                      address, write_ctx, word_aligned_reads_only)) {
      return false;
    }
  }
  return true;
}

static bool prv_write_coredump_sections(const sMemfaultCoredumpSaveInfo *save_info,
                                        bool compute_size_only, size_t *total_size) {
  sMfltCoredumpStorageInfo info = { 0 };
  sMfltCoredumpHeader hdr = { 0 };

  // are there some regions for us to save?
  size_t num_regions = save_info->num_regions;
  const sMfltCoredumpRegion *regions = save_info->regions;
  if ((regions == NULL) || (num_regions == 0)) {
    // sanity check that we got something valid from the caller
    return false;
  }

  if (!compute_size_only) {
    if (!memfault_platform_coredump_save_begin()) {
      return false;
    }

    // If we are saving a new coredump but one is already stored, don't overwrite it. This way
    // the first issue which started the crash loop can be determined
    MfltCoredumpReadCb coredump_read_cb = memfault_platform_coredump_storage_read;
    if (!prv_get_info_and_header(&hdr, &info, coredump_read_cb)) {
      return false;
    }

    if (prv_coredump_header_is_valid(&hdr)) {
      return false; // don't overwrite what we got!
    }
  }

  // erase storage provided we aren't just computing the size
  if (!compute_size_only && !memfault_platform_coredump_storage_erase(0, info.size)) {
    return false;
  }

  sMfltCoredumpWriteCtx write_ctx = {
    // We will write the header last as a way to mark validity
    // so advance the offset past it to start
    .offset = sizeof(hdr),
    .compute_size_only = compute_size_only,
    .storage_size = info.size,
  };

  if (write_ctx.storage_size > sizeof(sMfltCoredumpFooter)) {
    // always leave space for footer
    write_ctx.storage_size -= sizeof(sMfltCoredumpFooter);
  }

  const void *regs = save_info->regs;
  const size_t regs_size = save_info->regs_size;
  if (regs != NULL) {
    if (!prv_write_non_memory_block(kMfltCoredumpBlockType_CurrentRegisters,
                                    regs, regs_size, &write_ctx)) {
      return false;
    }
  }

  if (!prv_write_device_info_blocks(&write_ctx)) {
    return false;
  }

  const uint32_t trace_reason = save_info->trace_reason;
  if (!prv_write_trace_reason(&write_ctx, trace_reason)) {
    return false;
  }

  // write out any architecture specific regions
  size_t num_arch_regions = 0;
  const sMfltCoredumpRegion *arch_regions = memfault_coredump_get_arch_regions(&num_arch_regions);
  size_t num_sdk_regions = 0;
  const sMfltCoredumpRegion *sdk_regions = memfault_coredump_get_sdk_regions(&num_sdk_regions);

  const bool write_completed =
      prv_write_regions(&write_ctx, arch_regions, num_arch_regions) &&
      prv_write_regions(&write_ctx, sdk_regions, num_sdk_regions) &&
      prv_write_regions(&write_ctx, regions, num_regions);

  if (!write_completed && write_ctx.write_error) {
    return false;
  }

  const sMfltCoredumpFooter footer = (sMfltCoredumpFooter) {
    .magic = MEMFAULT_COREDUMP_FOOTER_MAGIC,
    .flags = write_ctx.truncated ? (1 << kMfltCoredumpBlockType_SaveTruncated) : 0,
  };
  write_ctx.storage_size = info.size;
  if (!prv_platform_coredump_write(&footer, sizeof(footer), &write_ctx)) {
    return false;
  }

  const size_t end_offset = write_ctx.offset;
  write_ctx.offset = 0; // we are writing the header so reset our write offset
  const bool success = prv_write_coredump_header(end_offset, &write_ctx);
  if (success) {
    *total_size = end_offset;
  }

  return success;
}

MEMFAULT_WEAK
bool memfault_platform_coredump_save_begin(void) {
  return true;
}

size_t memfault_coredump_get_save_size(const sMemfaultCoredumpSaveInfo *save_info) {
  const bool compute_size_only = true;
  size_t total_size = 0;
  prv_write_coredump_sections(save_info, compute_size_only, &total_size);
  return total_size;
}

bool memfault_coredump_save(const sMemfaultCoredumpSaveInfo *save_info) {
  const bool compute_size_only = false;
  size_t total_size = 0;
  return prv_write_coredump_sections(save_info, compute_size_only, &total_size);
}

bool memfault_coredump_has_valid_coredump(size_t *total_size_out) {
  sMfltCoredumpHeader hdr = { 0 };
  // This routine is only called while the system is running so _always_ use the
  // memfault_coredump_read, which is safe to call while the system is running
  MfltCoredumpReadCb coredump_read_cb = memfault_coredump_read;
  if (!prv_coredump_get_header(&hdr, coredump_read_cb)) {
    return false;
  }
  if (!prv_coredump_header_is_valid(&hdr)) {
    return false;
  }
  if (total_size_out) {
    *total_size_out = hdr.total_size;
  }
  return true;
}

MEMFAULT_WEAK
bool memfault_coredump_read(uint32_t offset, void *buf, size_t buf_len) {
  return memfault_platform_coredump_storage_read(offset, buf, buf_len);
}

//! Expose a data source for use by the Memfault Packetizer
const sMemfaultDataSourceImpl g_memfault_coredump_data_source = {
  .has_more_msgs_cb = memfault_coredump_has_valid_coredump,
  .read_msg_cb = memfault_coredump_read,
  .mark_msg_read_cb = memfault_platform_coredump_storage_clear,
};
