#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! A utility that implements a small subset of the CBOR RFC:
//!  https://tools.ietf.org/html/rfc7049
//!
//!
//! CONTEXT: The Memfault metric events API serializes data out to CBOR. Since the actual CBOR
//! serialization feature set needed by the SDK is a tiny subset of the CBOR RFC, a minimal
//! implementation is implemented here.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! The context used to track an active cbor encoding operation
//! A consumer of this API should never have to access the structure directly
typedef struct MemfaultCborEncoder sMemfaultCborEncoder;

//! The backing storage to write the encoded data to
//!
//! @param ctx The context provided as part of "memfault_cbor_encoder_init"

//! @param offset The offset within the storage to write to. These offsets are guaranteed to be
//!   sequential. (i.e If the last write wrote 3 bytes at offset 0, the next write_cb will begin at
//!   offset 3) The offset is returned as a convenience. For example if the backing storage is a RAM
//! buffer with no state-tracking of it's own
//! @param buf The payload to write to the storage
//! @param buf_len The size of the payload to write
typedef void (MemfaultCborWriteCallback)(void *ctx, uint32_t offset, const void *buf, size_t buf_len);

//! Initializes the 'encoder' structure. Must be called at the start of any new encoding
//!
//! @param encoder Context structure initialized by this routine for tracking active encoding state
//! @param write_cb The callback to be invoked when a write needs to be performed
//! @param context The context to be provided along with the write_cb
//! @param buf_len The space free in the backing storage being encoded to. The encoder API
//!  will _never_ attempt to write more bytes than this
void memfault_cbor_encoder_init(sMemfaultCborEncoder *encoder, MemfaultCborWriteCallback *cb,
                                void *context, size_t buf_len);

//! Same as "memfault_cbor_encoder_init" but instead of encoding to a buffer will
//! only set the encoder up to compute the total size of the encode
//!
//! When encoding is done and "memfault_cbort_encoder_deinit" is called the total
//! encoding size will be returned
void memfault_cbor_encoder_size_only_init(sMemfaultCborEncoder *encoder);

//! Resets the state of the encoder context
//!
//! @return the number of bytes successfully encoded
size_t memfault_cbor_encoder_deinit(sMemfaultCborEncoder *encoder);

//! Called to begin the encoding of a dictionary (also known as a map, object, hashes)
//!
//! @param encoder The encoder context to use
//! @param num_elements The number of pairs of data items that will be in the dictionary
//!
//! @return true on success, false otherwise
bool memfault_cbor_encode_dictionary_begin(sMemfaultCborEncoder *encoder, size_t num_elements);


//! Called to begin the encoding of an array (also referred to as a list, sequence, or tuple)
//!
//! @param encoder The encoder context to use
//! @param num_elements The number of data items that will be in the array
//!
//! @return true on success, false otherwise
bool memfault_cbor_encode_array_begin(sMemfaultCborEncoder *encoder, size_t num_elements);

//! Called to encode an unsigned 32-bit integer data item
//!
//! @param encoder The encoder context to use
//! @param value The value to store
//!
//! @return true on success, false otherwise
bool memfault_cbor_encode_unsigned_integer(sMemfaultCborEncoder *encoder, uint32_t value);

//! Same as "memfault_cbor_encode_unsigned_integer" but store an unsigned integer instead
bool memfault_cbor_encode_signed_integer(sMemfaultCborEncoder *encoder, int32_t value);

//! Adds pre-encoded cbor data to the current encoder
//!
//! @param encoder The encoder context to use
//! @param cbor_data The pre-encoded data to add to the current context
//! @param cbor_data_len The length of the pre-encoded data
//!
//! @note Care must be taken by the end user to ensure the data being joined into the current
//! encoding creates a valid cbor entry when combined. This utility can helpful, for example, when
//! adding a value to a cbor dictionary/map which is a cbor record itself.
bool memfault_cbor_join(sMemfaultCborEncoder *encoder, const void *cbor_data, size_t cbor_data_len);

//! Called to encode an arbitrary binary payload
//!
//! @param encoder The encoder context to use
//! @param buf The buffer to store
//! @param buf_len The length of the buffer to store
//!
//! @return true on success, false otherwise
bool memfault_cbor_encode_byte_string(sMemfaultCborEncoder *encoder, const void *buf,
                                      size_t buf_len);

//! Called to encode a NUL terminated C string
//!
//! @param encoder The encoder context to use
//! @param str The string to store
//!
//! @return true on success, false otherwise
bool memfault_cbor_encode_string(sMemfaultCborEncoder *encoder, const char *str);

//! Called to start the encoding of a C string
//!
//! @param encoder The encoder context to use
//! @param str_len The length of the string to store in bytes, excluding NULL terminator.
//!
//! @return true on success, false otherwise
//!
//! @note Use one or more calls to memfault_cbor_encode_string_add() to write the contents
//! of the string.
bool memfault_cbor_encode_string_begin(sMemfaultCborEncoder *encoder, size_t str_len);

//! Called to encode the given C string to the string started with
//! memfault_cbor_encode_string_begin().
//!
//! @note Can be called one or multiple times after calling memfault_cbor_encode_string_begin().
//! It is the responsibililty of the caller to ensure that the total number of bytes added matches
//! the str_len that was passed to memfault_cbor_encode_string_begin().
//! @note It is not expected to add the NULL terminator.
//!
//! @param encoder The encoder context to use
//! @param str The string to add
//! @param len The number of bytes to add from str
//!
//! @return true on success, false otherwise
bool memfault_cbor_encode_string_add(sMemfaultCborEncoder *encoder, const char *str, size_t len);

//! Encodes a IEEE 754 double-precision float that is packed in a uint64_t
//!
//! @param encoder The encoder context to use
//! @param val The value of the float to encode
//!
//! @return true on success, false otherwise
bool memfault_cbor_encode_uint64_as_double(sMemfaultCborEncoder *encoder, uint64_t value);


//! Called to encode a signed 64 bit data item
//!
//! @param encode The encoder context to use
//! @param value The value to store
//!
//! @return true on success, false otherwise
bool memfault_cbor_encode_long_signed_integer(sMemfaultCborEncoder *encoder, int64_t value);


//! NOTE: For internal use only, included in the header so it's easy for a caller to statically
//! allocate the structure
struct MemfaultCborEncoder {
  bool compute_size_only;
  MemfaultCborWriteCallback *write_cb;
  void *write_cb_ctx;
  size_t buf_len;

  size_t encoded_size;
};

#ifdef __cplusplus
}
#endif
