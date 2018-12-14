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
#include "modlog/modlog.h"
#include "mfg/mfg.h"

/**
 * The "manufacturing meta region" is located at the end of the boot loader
 * flash area.  This region has the following structure.
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   TLV type    |   TLV size    | TLV data ("TLV size" bytes)   ~
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               ~
 * ~                                                               ~
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   TLV type    |   TLV size    | TLV data ("TLV size" bytes)   ~
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               ~
 * ~                                                               ~
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          Region size          |    Version    | 0xff padding  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       Magic (0x3bb2a269)                      |
 * +-+-+-+-+-+--+-+-+-+-end of boot loader area+-+-+-+-+-+-+-+-+-+-+
 *
 * The number of TLVs is variable; two are shown above for illustrative
 * purposes.
 *
 * Fields:
 * <TLVs>
 * 1. TLV type: Indicates the type of data to follow.
 * 2. TLV size: The number of bytes of data to follow.
 * 3. TLV data: "TLV size" bytes of data.
 *
 * <Footer>
 * 4. Region size: The size, in bytes, of the entire manufacturing meta region;
 *    includes TLVs and footer.
 * 5. Version: Manufacturing meta version number; always 0x02.
 * 6. Magic: Indicates the presence of the manufacturing meta region.
 */

#define MFG_META_MAGIC          0x3bb2a269
#define MFG_META_VERSION        2
#define MFG_META_FOOTER_SZ      (sizeof (struct mfg_meta_footer))
#define MFG_META_TLV_SZ         (sizeof (struct mfg_meta_tlv))

struct mfg_meta_footer {
    uint16_t size;
    uint8_t version;
    uint8_t pad8;
    uint32_t magic;
};

/** Represents an MMR after it has been read from flash. */
struct mfg_mmr {
    /* Flash area containing MMR. */
    uint8_t area_id;

    /* Offset within flash area of start of MMR. */
    uint32_t offset;

    /* Total size of MMR (TLVs + footer). */
    uint32_t size;
};

/** The full set of MMRs comprised by all installed mfgimages. */
static struct mfg_mmr mfg_mmrs[MYNEWT_VAL(MFG_MAX_MMRS)];
static int mfg_num_mmrs;

/** True if MMR detection has occurred. */
static bool mfg_initialized;

void
mfg_open(struct mfg_reader *out_reader)
{
    /* Ensure MMRs have been detected. */
    mfg_init();

    /* Start at MMR index 0. */
    *out_reader = (struct mfg_reader) { 0 };
}

/**
 * Seeks to the next mfg TLV.
 *
 * @param reader                The reader to seek with.
 *
 * @return                      0 if the next TLV was successfully seeked to.
 *                              SYS_EDONE if there are no additional TLVs
 *                                  available.
 *                              SYS_EAGAIN if the end of the current MMR is
 *                                  reached, but additional MMRs are available
 *                                  for reading.
 *                              Other MFG error code on failure.
 */
static int
mfg_seek_next_aux(struct mfg_reader *reader)
{
    const struct flash_area *fap;
    const struct mfg_mmr *mmr;
    int rc;

    if (reader->mmr_idx >= mfg_num_mmrs) {
        /* The reader is expired. */
        return SYS_EINVAL;
    }

    mmr = &mfg_mmrs[reader->mmr_idx];

    rc = flash_area_open(mmr->area_id, &fap);
    if (rc != 0) {
        return SYS_EIO;
    }

    if (reader->offset == 0) {
        /* First seek; advance to the start of the MMR. */
        reader->offset = mmr->offset;
    } else {
        /* Follow-up seek; skip the current TLV. */
        reader->offset += MFG_META_TLV_SZ + reader->cur_tlv.size;
    }

    if (reader->offset >= fap->fa_size - MFG_META_FOOTER_SZ) {
        /* Reached end of the MMR; advance to the next MMR if one exists. */
        if (reader->mmr_idx + 1 >= mfg_num_mmrs) {
            rc = SYS_EDONE;
        } else {
            reader->offset = 0;
            reader->mmr_idx++;
            rc = SYS_EAGAIN;
        }
        goto done;
    }

    /* Read current TLV header. */
    rc = flash_area_read(fap, reader->offset, &reader->cur_tlv,
                         MFG_META_TLV_SZ);
    if (rc != 0) {
        rc = SYS_EIO;
        goto done;
    }

done:
    flash_area_close(fap);
    return rc;
}

int
mfg_seek_next(struct mfg_reader *reader)
{
    int rc;

    do {
        rc = mfg_seek_next_aux(reader);
    } while (rc == SYS_EAGAIN);

    return rc;
}

int
mfg_seek_next_with_type(struct mfg_reader *reader, uint8_t type)
{
    int rc;

    while (1) {
        rc = mfg_seek_next(reader);
        if (rc != 0) {
            break;
        }

        if (reader->cur_tlv.type == type) {
            break;
        }

        /* Proceed to next TLV. */
    }

    return rc;
}

/**
 * Opens the flash area pointed to by the provided reader.
 */
static int
mfg_open_flash_area(const struct mfg_reader *reader,
                    const struct flash_area **fap)
{
    const struct mfg_mmr *mmr;
    int rc;

    assert(reader->mmr_idx < mfg_num_mmrs);
    mmr = &mfg_mmrs[reader->mmr_idx];

    rc = flash_area_open(mmr->area_id, fap);
    if (rc != 0) {
        return SYS_EIO;
    }

    return 0;
}

static int
mfg_read_tlv_body(const struct mfg_reader *reader, void *dst, int max_size)
{
    const struct flash_area *fap;
    int read_sz;
    int rc;

    rc = mfg_open_flash_area(reader, &fap);
    if (rc != 0) {
        return rc;
    }

    memset(dst, 0, max_size);

    read_sz = min(max_size, reader->cur_tlv.size);
    rc = flash_area_read(fap, reader->offset + MFG_META_TLV_SZ, dst, read_sz);
    flash_area_close(fap);

    if (rc != 0) {
        return SYS_EIO;
    }

    return 0;
}

int
mfg_read_tlv_flash_area(const struct mfg_reader *reader,
                        struct mfg_meta_flash_area *out_mfa)
{
    return mfg_read_tlv_body(reader, out_mfa, sizeof *out_mfa);
}

int
mfg_read_tlv_mmr_ref(const struct mfg_reader *reader,
                     struct mfg_meta_mmr_ref *out_mr)
{
    return mfg_read_tlv_body(reader, out_mr, sizeof *out_mr);
}

int
mfg_read_tlv_hash(const struct mfg_reader *reader, void *out_hash)
{
    return mfg_read_tlv_body(reader, out_hash, MFG_HASH_SZ);
}

/**
 * Reads an MMR from the end of the specified flash area.
 */
static int
mfg_read_mmr(uint8_t area_id, struct mfg_mmr *out_mmr)
{
    const struct flash_area *fap;
    struct mfg_meta_footer ftr;
    int rc;

    rc = flash_area_open(area_id, &fap);
    if (rc != 0) {
        return SYS_EIO;
    }

    /* Read the MMR footer. */
    rc = flash_area_read(fap, fap->fa_size - sizeof ftr, &ftr, sizeof ftr);
    flash_area_close(fap);

    if (rc != 0) {
        return SYS_EIO;
    }

    if (ftr.magic != MFG_META_MAGIC) {
        return SYS_ENODEV;
    }

    if (ftr.version != MFG_META_VERSION) {
        return SYS_ENOTSUP;
    }

    if (ftr.size > fap->fa_size) {
        return SYS_ENODEV;
    }

    *out_mmr = (struct mfg_mmr) {
        .area_id = area_id,
        .offset = fap->fa_size - ftr.size,
        .size = ftr.size,
    };

    return 0;
}

/**
 * Reads an MMR from the end of the specified flash area.  On success, the
 * global MMR list is populated with the result for subsequent reading.
 */
static int
mfg_read_next_mmr(uint8_t area_id)
{
    int rc;
    int i;

    /* Detect if this MMR has already been read. */
    for (i = 0; i < mfg_num_mmrs; i++) {
        if (mfg_mmrs[i].area_id == area_id) {
            return SYS_EALREADY;
        }
    }

    if (mfg_num_mmrs >= MYNEWT_VAL(MFG_MAX_MMRS)) {
        return SYS_ENOMEM;
    }

    rc = mfg_read_mmr(area_id, &mfg_mmrs[mfg_num_mmrs]);
    if (rc != 0) {
        return rc;
    }

    mfg_num_mmrs++;
    return 0;
}

/**
 * Reads all MMR ref TLVs in the specified MMR.  The global MMR list is
 * populated with the results for subsequent reading.
 */
static int
mfg_read_mmr_refs(void)
{
    struct mfg_meta_mmr_ref mmr_ref;
    struct mfg_reader reader;
    int rc;

    mfg_open(&reader);

    /* Repeatedly find and read the next MMR ref TLV.  As new MMRs are read,
     * they are added to the global list and become available in this loop.
     */
    while (true) {
        rc = mfg_seek_next_with_type(&reader, MFG_META_TLV_TYPE_MMR_REF);
        switch (rc) {
        case 0:
            /* Found an MMR ref TLV.  Read it below. */
            break;

        case SYS_EDONE:
            /* No more MMR ref TLVs. */
            return 0;

        default:
            return rc;
        }

        rc = mfg_read_tlv_mmr_ref(&reader, &mmr_ref);
        if (rc != 0) {
            return rc;
        }

        rc = mfg_read_next_mmr(mmr_ref.area_id);
        if (rc != 0 && rc != SYS_EALREADY) {
            return rc;
        }
    }

    return 0;
}

/**
 * Locates the manufacturing meta region in flash.  This function must be
 * called before any TLVs can be read.  No-op if this function has already
 * executed successfully.
 */
void
mfg_init(void)
{
    int rc;

    if (mfg_initialized) {
        return;
    }
    mfg_initialized = true;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    /* Read the first MMR from the boot loader area. */
    rc = mfg_read_next_mmr(FLASH_AREA_BOOTLOADER);
    if (rc != 0) {
        goto err;
    }

    /* Read all MMR references. */
    rc = mfg_read_mmr_refs();
    if (rc != 0) {
        goto err;
    }

    return;

err:
    MFG_LOG(ERROR, "failed to read MMRs: rc=%d", rc);
}
