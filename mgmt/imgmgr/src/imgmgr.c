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
#include <os/endian.h>

#include <limits.h>
#include <assert.h>
#include <string.h>

#include "sysinit/sysinit.h"
#include "sysflash/sysflash.h"
#include "hal/hal_bsp.h"
#include "flash_map/flash_map.h"
#include "cborattr/cborattr.h"
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "mgmt/mgmt.h"

#include "imgmgr/imgmgr.h"
#include "imgmgr_priv.h"

static int imgr_upload(struct mgmt_cbuf *);
static int imgr_erase(struct mgmt_cbuf *);

static const struct mgmt_handler imgr_nmgr_handlers[] = {
    [IMGMGR_NMGR_ID_STATE] = {
        .mh_read = imgmgr_state_read,
        .mh_write = imgmgr_state_write,
    },
    [IMGMGR_NMGR_ID_UPLOAD] = {
        .mh_read = NULL,
        .mh_write = imgr_upload
    },
    [IMGMGR_NMGR_ID_ERASE] = {
        .mh_read = NULL,
        .mh_write = imgr_erase
    },
    [IMGMGR_NMGR_ID_CORELIST] = {
#if MYNEWT_VAL(IMGMGR_COREDUMP)
        .mh_read = imgr_core_list,
        .mh_write = NULL
#else
        .mh_read = NULL,
        .mh_write = NULL
#endif
    },
    [IMGMGR_NMGR_ID_CORELOAD] = {
#if MYNEWT_VAL(IMGMGR_COREDUMP)
        .mh_read = imgr_core_load,
        .mh_write = imgr_core_erase,
#else
        .mh_read = NULL,
        .mh_write = NULL
#endif
    },
};

#define IMGR_HANDLER_CNT                                                \
    sizeof(imgr_nmgr_handlers) / sizeof(imgr_nmgr_handlers[0])

static struct mgmt_group imgr_nmgr_group = {
    .mg_handlers = (struct mgmt_handler *)imgr_nmgr_handlers,
    .mg_handlers_count = IMGR_HANDLER_CNT,
    .mg_group_id = MGMT_GROUP_ID_IMAGE,
};

struct imgr_state imgr_state;

#if MYNEWT_VAL(BOOTUTIL_IMAGE_FORMAT_V2)
static int
imgr_img_tlvs(const struct flash_area *fa, struct image_header *hdr,
              uint32_t *start_off, uint32_t *end_off)
{
    struct image_tlv_info tlv_info;
    int rc;

    rc = flash_area_read(fa, *start_off, &tlv_info, sizeof(tlv_info));
    if (rc) {
        rc = -1;
        goto end;
    }
    if (tlv_info.it_magic != IMAGE_TLV_INFO_MAGIC) {
        rc = 1;
        goto end;
    }
    *start_off += sizeof(tlv_info);
    *end_off = *start_off + tlv_info.it_tlv_tot;
    rc = 0;
end:
    return rc;
}
#else
static int
imgr_img_tlvs(const struct flash_area *fa, struct image_header *hdr,
              uint32_t *start_off, uint32_t *end_off)
{
    *end_off = *start_off + hdr->ih_tlv_size;
    return 0;
}
#endif
/*
 * Read version and build hash from image located slot "image_slot".  Note:
 * this is a slot index, not a flash area ID.
 *
 * @param image_slot
 * @param ver (optional)
 * @param hash (optional)
 * @param flags
 *
 * Returns -1 if area is not readable.
 * Returns 0 if image in slot is ok, and version string is valid.
 * Returns 1 if there is not a full image.
 * Returns 2 if slot is empty. XXXX not there yet
 */
int
imgr_read_info(int image_slot, struct image_version *ver, uint8_t *hash,
               uint32_t *flags)
{
    struct image_header *hdr;
    struct image_tlv *tlv;
    int rc = -1;
    int rc2;
    const struct flash_area *fa;
    uint8_t data[sizeof(struct image_header)];
    uint32_t data_off, data_end;
    int area_id;

    area_id = flash_area_id_from_image_slot(image_slot);

    hdr = (struct image_header *)data;
    rc2 = flash_area_open(area_id, &fa);
    if (rc2) {
        return -1;
    }
    rc2 = flash_area_read(fa, 0, hdr, sizeof(*hdr));
    if (rc2) {
        goto end;
    }

    if (ver != NULL) {
        memset(ver, 0xff, sizeof(*ver));
        if (hdr->ih_magic == IMAGE_MAGIC) {
            memcpy(ver, &hdr->ih_ver, sizeof(*ver));
        } else if (hdr->ih_magic == 0xffffffff) {
            rc = 2;
            goto end;
        } else {
            rc = 1;
            goto end;
        }
    }

    if (flags) {
        *flags = hdr->ih_flags;
    }

    /*
     * Build ID is in a TLV after the image.
     */
    data_off = hdr->ih_hdr_size + hdr->ih_img_size;

    rc = imgr_img_tlvs(fa, hdr, &data_off, &data_end);
    if (rc) {
        goto end;
    }

    if (data_end > fa->fa_size) {
        rc = 1;
        goto end;
    }
    tlv = (struct image_tlv *)data;
    while (data_off + sizeof(*tlv) <= data_end) {
        rc2 = flash_area_read(fa, data_off, tlv, sizeof(*tlv));
        if (rc2) {
            goto end;
        }
        if (tlv->it_type == 0xff && tlv->it_len == 0xffff) {
            break;
        }
        if (tlv->it_type != IMAGE_TLV_SHA256 ||
          tlv->it_len != IMGMGR_HASH_LEN) {
            data_off += sizeof(*tlv) + tlv->it_len;
            continue;
        }
        data_off += sizeof(*tlv);
        if (hash) {
            if (data_off + IMGMGR_HASH_LEN > data_end) {
                goto end;
            }
            rc2 = flash_area_read(fa, data_off, hash, IMGMGR_HASH_LEN);
            if (rc2) {
                goto end;
            }
        }
        rc = 0;
        goto end;
    }
    rc = 1;
end:
    flash_area_close(fa);
    return rc;
}

int
imgr_my_version(struct image_version *ver)
{
    return imgr_read_info(boot_current_slot, ver, NULL, NULL);
}

/*
 * Finds image given version number. Returns the slot number image is in,
 * or -1 if not found.
 */
int
imgr_find_by_ver(struct image_version *find, uint8_t *hash)
{
    int i;
    struct image_version ver;

    for (i = 0; i < 2; i++) {
        if (imgr_read_info(i, &ver, hash, NULL) != 0) {
            continue;
        }
        if (!memcmp(find, &ver, sizeof(ver))) {
            return i;
        }
    }
    return -1;
}

/*
 * Finds image given hash of the image. Returns the slot number image is in,
 * or -1 if not found.
 */
int
imgr_find_by_hash(uint8_t *find, struct image_version *ver)
{
    int i;
    uint8_t hash[IMGMGR_HASH_LEN];

    for (i = 0; i < 2; i++) {
        if (imgr_read_info(i, ver, hash, NULL) != 0) {
            continue;
        }
        if (!memcmp(hash, find, IMGMGR_HASH_LEN)) {
            return i;
        }
    }
    return -1;
}

static int
imgr_erase(struct mgmt_cbuf *cb)
{
    struct image_version ver;
    int area_id;
    int best = -1;
    int rc;
    int i;
    CborError g_err = CborNoError;

    for (i = 0; i < 2; i++) {
        rc = imgr_read_info(i, &ver, NULL, NULL);
        if (rc < 0) {
            continue;
        }
        if (rc == 0) {
            /* Image in slot is ok. */
            if (imgmgr_state_slot_in_use(i)) {
                /* Slot is in use; can't erase to this. */
                continue;
            } else {
                /*
                 * Not active slot, but image is ok. Use it if there are
                 * no better candidates.
                 */
                best = i;
            }
            continue;
        }
        best = i;
        break;
    }
    if (best >= 0) {
        area_id = flash_area_id_from_image_slot(best);
        if (imgr_state.upload.fa) {
            flash_area_close(imgr_state.upload.fa);
            imgr_state.upload.fa = NULL;
        }
        rc = flash_area_open(area_id, &imgr_state.upload.fa);
        if (rc) {
            return MGMT_ERR_EINVAL;
        }
        rc = flash_area_erase(imgr_state.upload.fa, 0,
          imgr_state.upload.fa->fa_size);
        flash_area_close(imgr_state.upload.fa);
        imgr_state.upload.fa = NULL;
    } else {
        /*
         * No slot where to erase!
         */
        return MGMT_ERR_ENOMEM;
    }

    if (!imgr_state.upload.fa) {
        return MGMT_ERR_EINVAL;
    }

    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return 0;
}

static int
imgr_upload(struct mgmt_cbuf *cb)
{
    uint8_t img_data[MYNEWT_VAL(IMGMGR_MAX_CHUNK_SIZE)];
    long long unsigned int off = UINT_MAX;
    long long unsigned int size = UINT_MAX;
    size_t data_len = 0;
    const struct cbor_attr_t off_attr[4] = {
        [0] = {
            .attribute = "data",
            .type = CborAttrByteStringType,
            .addr.bytestring.data = img_data,
            .addr.bytestring.len = &data_len,
            .len = sizeof(img_data)
        },
        [1] = {
            .attribute = "len",
            .type = CborAttrUnsignedIntegerType,
            .addr.uinteger = &size,
            .nodefault = true
        },
        [2] = {
            .attribute = "off",
            .type = CborAttrUnsignedIntegerType,
            .addr.uinteger = &off,
            .nodefault = true
        },
        [3] = { 0 },
    };
    struct image_version ver;
    struct image_header *hdr;
    int area_id;
    int best;
    int rc;
    int i;
    bool empty = false;
    CborError g_err = CborNoError;

    rc = cbor_read_object(&cb->it, off_attr);
    if (rc || off == UINT_MAX) {
        return MGMT_ERR_EINVAL;
    }

    if (off == 0) {
        if (data_len < sizeof(struct image_header)) {
            /*
             * Image header is the first thing in the image.
             */
            return MGMT_ERR_EINVAL;
        }
        hdr = (struct image_header *)img_data;
        if (hdr->ih_magic != IMAGE_MAGIC) {
            return MGMT_ERR_EINVAL;
        }

        /*
         * New upload.
         */
        imgr_state.upload.off = 0;
        imgr_state.upload.size = size;
        best = -1;

        for (i = 0; i < 2; i++) {
            rc = imgr_read_info(i, &ver, NULL, NULL);
            if (rc < 0) {
                continue;
            }
            if (rc == 0) {
                /* Image in slot is ok. */
                if (imgmgr_state_slot_in_use(i)) {
                    /* Slot is in use; can't upload to this. */
                    continue;
                } else {
                    /*
                     * Not active slot, but image is ok. Use it if there are
                     * no better candidates.
                     */
                    best = i;
                }
                continue;
            }
            best = i;
            break;
        }
        if (best >= 0) {
            area_id = flash_area_id_from_image_slot(best);
            if (imgr_state.upload.fa) {
                flash_area_close(imgr_state.upload.fa);
                imgr_state.upload.fa = NULL;
            }
            rc = flash_area_open(area_id, &imgr_state.upload.fa);
            if (rc) {
                return MGMT_ERR_EINVAL;
            }

            rc = flash_area_is_empty(imgr_state.upload.fa, &empty);
            if (rc) {
                return MGMT_ERR_EINVAL;
            }

            if(!empty) {
                rc = flash_area_erase(imgr_state.upload.fa, 0,
                  imgr_state.upload.fa->fa_size);
            }
        } else {
            /*
             * No slot where to upload!
             */
            return MGMT_ERR_ENOMEM;
        }
    } else if (off != imgr_state.upload.off) {
        /*
         * Invalid offset. Drop the data, and respond with the offset we're
         * expecting data for.
         */
        goto out;
    }

    if (!imgr_state.upload.fa) {
        return MGMT_ERR_EINVAL;
    }
    if (data_len) {
        rc = flash_area_write(imgr_state.upload.fa, imgr_state.upload.off,
          img_data, data_len);
        if (rc) {
            rc = MGMT_ERR_EINVAL;
            goto err_close;
        }
        imgr_state.upload.off += data_len;
        if (imgr_state.upload.size == imgr_state.upload.off) {
            /* Done */
            flash_area_close(imgr_state.upload.fa);
            imgr_state.upload.fa = NULL;
        }
    }

out:
    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);
    g_err |= cbor_encode_text_stringz(&cb->encoder, "off");
    g_err |= cbor_encode_int(&cb->encoder, imgr_state.upload.off);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return 0;
err_close:
    flash_area_close(imgr_state.upload.fa);
    imgr_state.upload.fa = NULL;
    return rc;
}

void
imgmgr_module_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = mgmt_group_register(&imgr_nmgr_group);
    SYSINIT_PANIC_ASSERT(rc == 0);

#if MYNEWT_VAL(IMGMGR_CLI)
    rc = imgr_cli_register();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
}
