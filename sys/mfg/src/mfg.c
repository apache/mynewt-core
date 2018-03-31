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

#include <string.h>
#include "os/mynewt.h"
#include "mfg/mfg.h"

/**
 * The "manufacturing meta region" is located at the end of the boot loader
 * flash area.  This region has the following structure.
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |Version (0x01) |                  0xff padding                 |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   TLV type    |   TLV size    | TLV data ("TLV size" bytes)   ~
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               ~
 * ~                                                               ~
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   TLV type    |   TLV size    | TLV data ("TLV size" bytes)   ~
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               ~
 * ~                                                               ~
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   Region size                 |         0xff padding          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       Magic (0x3bb2a269)                      |
 * +-+-+-+-+-+--+-+-+-+-end of boot loader area+-+-+-+-+-+-+-+-+-+-+
 *
 * The number of TLVs is variable; two are shown above for illustrative
 * purposes.
 *
 * Fields:
 * <Header>
 * 1. Version: Manufacturing meta version number; always 0x01.
 *
 * <TLVs>
 * 2. TLV type: Indicates the type of data to follow.
 * 3. TLV size: The number of bytes of data to follow.
 * 4. TLV data: "TLV size" bytes of data.
 *
 * <Footer>
 * 5. Region size: The size, in bytes, of the entire manufacturing meta region;
 *    includes header, TLVs, and footer.
 * 6. Magic: indicates the presence of the manufacturing meta region.
 */

#define MFG_META_MAGIC          0x3bb2a269
#define MFG_META_HEADER_SZ      4
#define MFG_META_FOOTER_SZ      8
#define MFG_META_TLV_SZ         2
#define MFG_META_FLASH_AREA_SZ  12

struct {
    uint8_t valid:1;
    uint32_t off;
    uint32_t size;
} mfg_state;

struct mfg_meta_header {
    uint8_t version;
    uint8_t pad8;
    uint16_t pad16;
};

struct mfg_meta_footer {
    uint16_t size;
    uint16_t pad16;
    uint32_t magic;
};

_Static_assert(sizeof (struct mfg_meta_header) == MFG_META_HEADER_SZ,
               "mfg_meta_header must be 4 bytes");
_Static_assert(sizeof (struct mfg_meta_footer) == MFG_META_FOOTER_SZ,
               "mfg_meta_footer must be 8 bytes");
_Static_assert(sizeof (struct mfg_meta_flash_area) == MFG_META_FLASH_AREA_SZ,
               "mfg_meta_flash_area must be 12 bytes");

/**
 * Retrieves a TLV header from the mfg meta region.  To request the first TLV
 * in the region, specify an offset of 0.  To request a subsequent TLV, specify
 * the values retrieved by the previous call to this function.
 *
 * @param tlv (in / out)        Input: The previously-read TLV header; not used
 *                                  as input when requesting the first TLV.
 *                              Output: On success, the requested TLV header
 *                                  gets written here.
 * @param off (in / out)        Input: The flash-area-offset of the previously
 *                                  read TLV header; 0 when requesting the
 *                                  first TLV.
 *                              Output: On success, the flash-area-offset of
 *                                  the retrieved TLV header.
 *
 * @return                      0 if a TLV header was successfully retrieved;
 *                              MFG_EDONE if there are no additional TLVs to
 *                                  read;
 *                              Other MFG error code on failure.
 */
int
mfg_next_tlv(struct mfg_meta_tlv *tlv, uint32_t *off)
{
    const struct flash_area *fap;
    int rc;

    if (!mfg_state.valid) {
        return MFG_EUNINIT;
    }

    rc = flash_area_open(FLASH_AREA_BOOTLOADER, &fap);
    if (rc != 0) {
        rc = MFG_EFLASH;
        goto done;
    }

    if (*off == 0) {
        *off = mfg_state.off + MFG_META_HEADER_SZ;
    } else {
        /* Advance past current TLV. */
        *off += MFG_META_TLV_SZ + tlv->size;
    }

    if (*off + MFG_META_FOOTER_SZ >= fap->fa_size) {
        /* Reached end of boot area; no more TLVs. */
        return MFG_EDONE;
    }

    /* Read next TLV. */
    memset(tlv, 0, sizeof *tlv);
    rc = flash_area_read(fap, *off, tlv, MFG_META_TLV_SZ);
    if (rc != 0) {
        rc = MFG_EFLASH;
        goto done;
    }

    rc = 0;

done:
    flash_area_close(fap);
    return rc;
}

/**
 * Retrieves a TLV header of the specified type from the mfg meta region.  To
 * request the first TLV in the region, specify an offset of 0.  To request a
 * subsequent TLV, specify the values retrieved by the previous call to this
 * function.
 *
 * @param tlv (in / out)        Input: The previously-read TLV header; not used
 *                                  as input when requesting the first TLV.
 *                              Output: On success, the requested TLV header
 *                                  gets written here.
 * @param off (in / out)        Input: The flash-area-offset of the previously
 *                                  read TLV header; 0 when requesting the
 *                                  first TLV.
 *                              Output: On success, the flash-area-offset of
 *                                  the retrieved TLV header.
 * @param type                  The type of TLV to retrieve; one of the
 *                                  MFG_META_TLV_TYPE_[...] constants.
 *
 * @return                      0 if a TLV header was successfully retrieved;
 *                              MFG_EDONE if there are no additional TLVs of
 *                                  the specified type to read;
 *                              Other MFG error code on failure.
 */
int
mfg_next_tlv_with_type(struct mfg_meta_tlv *tlv, uint32_t *off, uint8_t type)
{
    int rc;

    while (1) {
        rc = mfg_next_tlv(tlv, off);
        if (rc != 0) {
            break;
        }

        if (tlv->type == type) {
            break;
        }

        /* Proceed to next TLV. */
    }

    return rc;
}

/**
 * Reads a flash-area TLV from the manufacturing meta region.  This function
 * should only be called after a TLV has been identified as having the
 * MFG_META_TLV_TYPE_FLASH_AREA type.
 *
 * @param tlv                   The header of the TLV to read.  This header
 *                                  should have been retrieved via a call to
 *                                  mfg_next_tlv() or mfg_next_tlv_with_type().
 * @param off                   The flash-area-offset of the TLV header.  Note:
 *                                  this is the offset of the TLV header, not
 *                                  the TLV data.
 * @param out_mfa (out)         On success, the retrieved flash area
 *                                  information gets written here.
 *
 * @return                      0 on success; MFG error code on failure.
 */
int
mfg_read_tlv_flash_area(const struct mfg_meta_tlv *tlv, uint32_t off,
                        struct mfg_meta_flash_area *out_mfa)
{
    const struct flash_area *fap;
    int read_sz;
    int rc;

    rc = flash_area_open(FLASH_AREA_BOOTLOADER, &fap);
    if (rc != 0) {
        rc = MFG_EFLASH;
        goto done;
    }

    memset(out_mfa, 0, sizeof *out_mfa);

    read_sz = min(MFG_META_FLASH_AREA_SZ, tlv->size);
    rc = flash_area_read(fap, off + MFG_META_TLV_SZ, out_mfa, read_sz);
    if (rc != 0) {
        rc = MFG_EFLASH;
        goto done;
    }

    rc = 0;

done:
    flash_area_close(fap);
    return rc;
}

/**
 * Reads a hash TLV from the manufacturing meta region.  This function should
 * only be called after a TLV has been identified as having the
 * MFG_META_TLV_TYPE_HASH type.
 *
 * @param tlv                   The header of the TLV to read.  This header
 *                                  should have been retrieved via a call to
 *                                  mfg_next_tlv() or mfg_next_tlv_with_type().
 * @param off                   The flash-area-offset of the TLV header.  Note:
 *                                  this is the offset of the TLV header, not
 *                                  the TLV data.
 * @param out_hash (out)        On success, the retrieved SHA256 hash gets
 *                                  written here.  This buffer must be at least
 *                                  32 bytes wide.
 *
 * @return                      0 on success; MFG error code on failure.
 */
int
mfg_read_tlv_hash(const struct mfg_meta_tlv *tlv, uint32_t off, void *out_hash)
{
    const struct flash_area *fap;
    int read_sz;
    int rc;

    rc = flash_area_open(FLASH_AREA_BOOTLOADER, &fap);
    if (rc != 0) {
        rc = MFG_EFLASH;
        goto done;
    }

    read_sz = min(MFG_HASH_SZ, tlv->size);
    rc = flash_area_read(fap, off + MFG_META_TLV_SZ, out_hash, read_sz);
    if (rc != 0) {
        rc = MFG_EFLASH;
        goto done;
    }

    rc = 0;

done:
    flash_area_close(fap);
    return rc;
}

/**
 * Locates the manufacturing meta region in flash.  This function must be
 * called before any TLVs can be read.  No-op if this function has already
 * executed successfully.
 *
 * @return                      0 on success; MFG error code on failure.
 */
int
mfg_init(void)
{
    const struct flash_area *fap;
    struct mfg_meta_footer ftr;
    uint16_t off;
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    if (mfg_state.valid) {
        /* Already initialized. */
        return 0;
    }

    mfg_state.valid = 0;

    rc = flash_area_open(FLASH_AREA_BOOTLOADER, &fap);
    if (rc != 0) {
        rc = MFG_EFLASH;
        goto done;
    }

    off = fap->fa_size - sizeof ftr;
    rc = flash_area_read(fap, off, &ftr, sizeof ftr);
    if (rc != 0) {
        rc = MFG_EFLASH;
        goto done;
    }

    if (ftr.magic != MFG_META_MAGIC) {
        rc = MFG_EBADDATA;
        goto done;
    }
    if (ftr.size > fap->fa_size) {
        rc = MFG_EBADDATA;
        goto done;
    }

    mfg_state.valid = 1;
    mfg_state.off = fap->fa_size - ftr.size;
    mfg_state.size = ftr.size;

    rc = 0;

done:
    flash_area_close(fap);
    return rc;
}
