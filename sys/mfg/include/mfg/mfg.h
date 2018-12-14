/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef H_MFG_
#define H_MFG_

#define MFG_HASH_SZ                     32

#define MFG_META_TLV_TYPE_HASH          0x01
#define MFG_META_TLV_TYPE_FLASH_AREA    0x02

/** Informational only; not read by firmware. */
#define MFG_META_TLV_TYPE_FLASH_TRAITS  0x03

#define MFG_META_TLV_TYPE_MMR_REF       0x04

struct mfg_meta_tlv {
    uint8_t type;
    uint8_t size;
    /* Followed by packed data. */
} __attribute__((packed));

struct mfg_meta_flash_area {
    uint8_t area_id;
    uint8_t device_id;
    uint32_t offset;
    uint32_t size;
} __attribute__((packed));

/** Informational only; not read by firmware. */
struct mfg_meta_flash_traits {
    uint8_t device_id;
    uint8_t min_write_sz;
} __attribute__((packed));

struct mfg_meta_mmr_ref {
    uint8_t area_id;
} __attribute__((packed));

/**
 * Object used for reading records from the manufacturing space.  The
 * `mfg_open()` function should be used to construct a reader object.
 */
struct mfg_reader {
    /** Public (read-only). */
    struct mfg_meta_tlv cur_tlv;

    /** Private. */
    uint8_t mmr_idx;
    uint32_t offset;
};

/**
 * Opens the manufacturing space for reading.  The resulting `mfg_reader`
 * object should be passed to subsequent seek and read functions.
 */
void mfg_open(struct mfg_reader *out_reader);

/**
 * Seeks to the next mfg TLV.  The caller must initialize the supplied
 * `mfg_reader` with `mfg_open()` prior to calling this function.
 *
 * @param reader                The reader to seek with.
 *
 * @return                      0 if the next TLV was successfully seeked to.
 *                              SYS_EDONE if there are no additional TLVs
 *                                  available.
 *                              Other MFG error code on failure.
 */
int mfg_seek_next(struct mfg_reader *reader);
/**
 * Seeks to the next mfg TLV with the specified type.  The caller must
 * initialize the supplied `mfg_reader` with `mfg_open()` prior to calling this
 * function.
 *
 * @param reader                The reader to seek with.
 * @param type                  The type of TLV to seek to; one of the
 *                                  MFG_META_TLV_TYPE_[...] constants.
 *
 * @return                      0 if the next TLV was successfully seeked to.
 *                              SYS_EDONE if there are no additional TLVs
 *                                  with the specified type available.
 *                              Other MFG error code on failure.
 */
int mfg_seek_next_with_type(struct mfg_reader *reader, uint8_t type);

/**
 * Reads a hash TLV from the manufacturing space.  This function should
 * only be called when the provided reader is pointing at a TLV with the
 * MFG_META_TLV_TYPE_HASH type.
 *
 * @param reader                The reader to read with.
 * @param out_mr (out)          On success, the retrieved MMR reference
 *                                  information gets written here.
 *
 * @return                      0 on success; MFG error code on failure.
 */
int mfg_read_tlv_hash(const struct mfg_reader *reader, void *out_hash);

/**
 * Reads a flash-area TLV from the manufacturing space.  This function should
 * only be called when the provided reader is pointing at a TLV with the
 * MFG_META_TLV_TYPE_FLASH_AREA type.
 *
 * @param reader                The reader to read with.
 * @param out_mfa (out)         On success, the retrieved flash area
 *                                  information gets written here.
 *
 * @return                      0 on success; MFG error code on failure.
 */
int mfg_read_tlv_flash_area(const struct mfg_reader *reader,
                            struct mfg_meta_flash_area *out_mfa);

/**
 * Reads an MMR ref TLV from the manufacturing space.  This function should
 * only be called when the provided reader is pointing at a TLV with the
 * MFG_META_TLV_TYPE_MMR_REF type.
 *
 * @param reader                The reader to read with.
 * @param out_mr (out)          On success, the retrieved MMR reference
 *                                  information gets written here.
 *
 * @return                      0 on success; MFG error code on failure.
 */
int mfg_read_tlv_mmr_ref(const struct mfg_reader *reader,
                         struct mfg_meta_mmr_ref *out_mr);

/**
 * Initializes the mfg package.
 */
void mfg_init(void);

#define MFG_LOG(lvl_, ...) \
    MODLOG_ ## lvl_(MYNEWT_VAL(MFG_LOG_MODULE), __VA_ARGS__)

#endif
