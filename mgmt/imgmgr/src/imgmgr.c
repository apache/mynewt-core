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

#include <limits.h>
#include <assert.h>
#include <string.h>

#include "os/mynewt.h"
#include "hal/hal_bsp.h"
#include "flash_map/flash_map.h"
#include "cborattr/cborattr.h"
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "mgmt/mgmt.h"
#if MYNEWT_VAL(LOG_FCB_SLOT1)
#include "log/log_fcb_slot1.h"
#endif

#include "imgmgr/imgmgr.h"
#include "imgmgr_priv.h"

const imgmgr_dfu_callbacks_t *imgmgr_dfu_callbacks_fn;

static int imgr_upload(struct mgmt_cbuf *);
static int imgr_erase(struct mgmt_cbuf *);
static int imgr_erase_state(struct mgmt_cbuf *);

/** Represents an individual upload request. */
struct imgr_upload_req {
    unsigned long long int off;     /* -1 if unspecified */
    unsigned long long int size;    /* -1 if unspecified */
    size_t data_len;
    size_t data_sha_len;
    uint8_t img_data[MYNEWT_VAL(IMGMGR_MAX_CHUNK_SIZE)];
    uint8_t data_sha[IMGMGR_DATA_SHA_LEN];
    bool upgrade;                   /* Only allow greater version numbers. */
};

/** Describes what to do during processing of an upload request. */
struct imgr_upload_action {
    /** The total size of the image. */
    unsigned long long size;

    /** The number of image bytes to write to flash. */
    int write_bytes;

    /** The flash area to write to. */
    int area_id;

    /** Whether to process the request; false if offset is wrong. */
    bool proceed;

    /** Whether to erase the destination flash area. */
    bool erase;
};

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
    [IMGMGR_NMGR_ID_ERASE_STATE] = {
        .mh_read = NULL,
        .mh_write = imgr_erase_state,
    },
};

#define IMGR_HANDLER_CNT                                                \
    sizeof(imgr_nmgr_handlers) / sizeof(imgr_nmgr_handlers[0])

static struct mgmt_group imgr_nmgr_group = {
    .mg_handlers = (struct mgmt_handler *)imgr_nmgr_handlers,
    .mg_handlers_count = IMGR_HANDLER_CNT,
    .mg_group_id = MGMT_GROUP_ID_IMAGE,
};

/** Global state for upload in progress. */
static struct {
    /** Flash area being written; -1 if no upload in progress. */
    int area_id;

    /** Flash offset of next chunk. */
    uint32_t off;

    /** Total size of image data. */
    uint32_t size;

    /** Hash of image data; used for resumption of a partial upload. */
    uint8_t data_sha_len;
    uint8_t data_sha[IMGMGR_DATA_SHA_LEN];
#if MYNEWT_VAL(IMGMGR_LAZY_ERASE)
    int sector_id;
    uint32_t sector_end;
#endif

} imgr_state;

static imgr_upload_fn *imgr_upload_cb;
static void *imgr_upload_arg;

#if MYNEWT_VAL(IMGMGR_VERBOSE_ERR)
static const char *imgmgr_err_str_app_reject = "app reject";
static const char *imgmgr_err_str_hdr_malformed = "header malformed";
static const char *imgmgr_err_str_magic_mismatch = "magic mismatch";
static const char *imgmgr_err_str_no_slot = "no slot";
static const char *imgmgr_err_str_flash_open_failed = "fa open fail";
static const char *imgmgr_err_str_flash_erase_failed = "fa erase fail";
static const char *imgmgr_err_str_flash_write_failed = "fa write fail";
static const char *imgmgr_err_str_downgrade = "downgrade";
#else
#define imgmgr_err_str_app_reject                   NULL
#define imgmgr_err_str_hdr_malformed                NULL
#define imgmgr_err_str_magic_mismatch               NULL
#define imgmgr_err_str_no_slot                      NULL
#define imgmgr_err_str_flash_open_failed            NULL
#define imgmgr_err_str_flash_erase_failed           NULL
#define imgmgr_err_str_flash_write_failed           NULL
#define imgmgr_err_str_downgrade                    NULL
#endif

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
 * @param flags (optional)
 *
 * Returns -1 if area is not readable.
 * Returns 0 if image in slot is ok, and version string is valid.
 * Returns 1 if there is not a full image.
 * Returns 2 if slot is empty.
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

    /* Silence spurious warning. */
    data_end = 0;

    area_id = flash_area_id_from_image_slot(image_slot);

    hdr = (struct image_header *)data;
    rc2 = flash_area_open(area_id, &fa);
    if (rc2) {
        return -1;
    }
    rc2 = flash_area_read_is_empty(fa, 0, hdr, sizeof(*hdr));
    if (rc2 < 0) {
        goto end;
    }

    if (ver) {
        memset(ver, 0xff, sizeof(*ver));
    }
    if (hdr->ih_magic == IMAGE_MAGIC) {
        if (ver) {
            memcpy(ver, &hdr->ih_ver, sizeof(*ver));
        }
    } else if (rc2 == 1) {
        /* Area is empty */
        rc = 2;
        goto end;
    } else {
        rc = 1;
        goto end;
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
        rc2 = flash_area_read_is_empty(fa, data_off, tlv, sizeof(*tlv));
        if (rc2 < 0) {
            goto end;
        }
        if (rc2 == 1) {
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

/**
 * Compares two image version numbers in a semver-compatible way.
 *
 * @param a                     The first version to compare.
 * @param b                     The second version to compare.
 *
 * @return                      -1 if a < b
 * @return                       0 if a = b
 * @return                       1 if a > b
 */
static int
imgr_vercmp(const struct image_version *a, const struct image_version *b)
{
    if (a->iv_major < b->iv_major) {
        return -1;
    } else if (a->iv_major > b->iv_major) {
        return 1;
    }

    if (a->iv_minor < b->iv_minor) {
        return -1;
    } else if (a->iv_minor > b->iv_minor) {
        return 1;
    }

    if (a->iv_revision < b->iv_revision) {
        return -1;
    } else if (a->iv_revision > b->iv_revision) {
        return 1;
    }

    /* Note: For semver compatibility, don't compare the 32-bit build num. */

    return 0;
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

int
imgmgr_find_best_area_id(void)
{
    struct image_version ver;
    int best = -1;
    int i;
    int rc;

    for (i = 0; i < 2; i++) {
        rc = imgr_read_info(i, &ver, NULL, NULL);
        if (rc < 0) {
            continue;
        }
        if (rc == 0) {
            /* Image in slot is ok. */
            if (imgmgr_state_slot_in_use(i)) {
                /* Slot is in use; can't use this. */
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
        best = flash_area_id_from_image_slot(best);
    }
    return best;
}

#if MYNEWT_VAL(IMGMGR_VERBOSE_ERR)
static int
imgr_error_rsp(struct mgmt_cbuf *cb, int rc, const char *rsn)
{
    /*
     * This is an error response so returning a different error when failed to
     * encode other error probably does not make much sense - just ignore errors
     * here.
     */
    cbor_encode_text_stringz(&cb->encoder, "rsn");
    cbor_encode_text_stringz(&cb->encoder, rsn);

    return rc;
}
#else
#define imgr_error_rsp(cb, rc, rsn)         (rc)
#endif

static int
imgr_erase(struct mgmt_cbuf *cb)
{
    const struct flash_area *fa;
    int area_id;
    int rc;
    CborError g_err = CborNoError;

    area_id = imgmgr_find_best_area_id();
    if (area_id >= 0) {
#if MYNEWT_VAL(LOG_FCB_SLOT1)
        /*
         * If logging to slot1 is enabled, make sure it's locked before erasing
         * so log handler does not corrupt our data.
         */
        if (area_id == FLASH_AREA_IMAGE_1) {
            log_fcb_slot1_lock();
        }
#endif

        rc = flash_area_open(area_id, &fa);
        if (rc) {
            return imgr_error_rsp(cb, MGMT_ERR_EINVAL,
                                  imgmgr_err_str_flash_open_failed);
        }
        rc = flash_area_erase(fa, 0, fa->fa_size);
        flash_area_close(fa);
        if (rc) {
            return imgr_error_rsp(cb, MGMT_ERR_EINVAL,
                                  imgmgr_err_str_flash_erase_failed);
        }
    } else {
        /*
         * No slot where to erase!
         */
        return imgr_error_rsp(cb, MGMT_ERR_ENOMEM, imgmgr_err_str_no_slot);
    }

    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }

    /* Reset in-progress upload. */
    imgr_state.area_id = -1;

    return 0;
}

static int
imgr_erase_state(struct mgmt_cbuf *cb)
{
    const struct flash_area *fa;
    int area_id;
    int rc;
    CborError g_err = CborNoError;

    area_id = imgmgr_find_best_area_id();
    if (area_id >= 0) {
        rc = flash_area_open(area_id, &fa);
        if (rc) {
            return imgr_error_rsp(cb, MGMT_ERR_EINVAL,
                                  imgmgr_err_str_flash_open_failed);
        }

        rc = flash_area_erase(fa, 0, sizeof(struct image_header));
        if (rc) {
            return imgr_error_rsp(cb, MGMT_ERR_EINVAL,
                                  imgmgr_err_str_flash_erase_failed);
        }

        flash_area_close(fa);

#if MYNEWT_VAL(LOG_FCB_SLOT1)
        /* If logging to slot1 is enabled, we can unlock it now. */
        if (area_id == FLASH_AREA_IMAGE_1) {
            log_fcb_slot1_unlock();
        }
#endif
    } else {
        return imgr_error_rsp(cb, MGMT_ERR_ENOMEM, imgmgr_err_str_no_slot);
    }

    g_err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    g_err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }

    /* Reset in-progress upload. */
    imgr_state.area_id = -1;

    return 0;
}

#if MYNEWT_VAL(IMGMGR_LAZY_ERASE)

/**
 * Erases a flash sector as image upload crosses a sector boundary.
 * Erasing the entire flash size at one time can take significant time,
 *   causing a bluetooth disconnect or significant battery sag.
 * Instead we will erase immediately prior to crossing a sector.
 * We could check for empty to increase efficiency, but instead we always erase
 *   for consistency and simplicity.
 *
 * @param fa       Flash area being traversed
 * @param off      Offset that is about to be written
 * @param len      Number of bytes to be written
 *
 * @return         0 if success 
 *                 ERROR_CODE if could not erase sector
 */
int
imgr_erase_if_needed(const struct flash_area *fa, uint32_t off, uint32_t len)
{
    int rc = 0;
    struct flash_area sector;

    while ((fa->fa_off + off + len) > imgr_state.sector_end) {
        rc = flash_area_getnext_sector(fa->fa_id, &imgr_state.sector_id, &sector);
        if (rc) {
            return rc;
        }
        rc = flash_area_erase(&sector, 0, sector.fa_size);
        if (rc) {
            return rc;
        }
        imgr_state.sector_end = sector.fa_off + sector.fa_size;
    }
    return 0;
}
#endif

/**
 * Verifies an upload request and indicates the actions that should be taken
 * during processing of the request.  This is a "read only" function in the
 * sense that it doesn't write anything to flash and doesn't modify any global
 * variables.
 *
 * @param req                   The upload request to inspect.
 * @param action                On success, gets populated with information
 *                                  about how to process the request.
 *
 * @return                      0 if processing should occur;
 *                              A MGMT_ERR code if an error response should be
 *                                  sent instead.
 */
static int
imgr_upload_inspect(const struct imgr_upload_req *req,
                    struct imgr_upload_action *action, const char **errstr)
{
    const struct image_header *hdr;
    const struct flash_area *fa;
    struct image_version cur_ver;
    uint8_t rem_bytes;
    bool empty;
    int rc;

    memset(action, 0, sizeof *action);

    if (req->off == -1) {
        /* Request did not include an `off` field. */
        *errstr = imgmgr_err_str_hdr_malformed;
        return MGMT_ERR_EINVAL;
    }

    if (req->off == 0) {
        /* First upload chunk. */
        if (req->data_len < sizeof(struct image_header)) {
            /*
             * Image header is the first thing in the image.
             */
            *errstr = imgmgr_err_str_hdr_malformed;
            return MGMT_ERR_EINVAL;
        }

        if (req->size == -1) {
            /* Request did not include a `len` field. */
            *errstr = imgmgr_err_str_hdr_malformed;
            return MGMT_ERR_EINVAL;
        }
        action->size = req->size;

        hdr = (struct image_header *)req->img_data;
        if (hdr->ih_magic != IMAGE_MAGIC) {
            *errstr = imgmgr_err_str_magic_mismatch;
            return MGMT_ERR_EINVAL;
        }

        if (req->data_sha_len > IMGMGR_DATA_SHA_LEN) {
            return MGMT_ERR_EINVAL;
        }

        /*
         * If request includes proper data hash we can check whether there is
         * upload in progress (interrupted due to e.g. link disconnection) with
         * the same data hash so we can just resume it by simply including
         * current upload offset in response.
         */
        if ((req->data_sha_len > 0) && (imgr_state.area_id != -1)) {
            if ((imgr_state.data_sha_len == req->data_sha_len) &&
                            !memcmp(imgr_state.data_sha, req->data_sha,
                                                        req->data_sha_len)) {
                return 0;
            }
        }

        action->area_id = imgmgr_find_best_area_id();
        if (action->area_id < 0) {
            /* No slot where to upload! */
            *errstr = imgmgr_err_str_no_slot;
            return MGMT_ERR_ENOMEM;
        }

        if (req->upgrade) {
            /* User specified upgrade-only.  Make sure new image version is
             * greater than that of the currently running image.
             */
            rc = imgr_my_version(&cur_ver);
            if (rc != 0) {
                return MGMT_ERR_EUNKNOWN;
            }

            if (imgr_vercmp(&cur_ver, &hdr->ih_ver) >= 0) {
                *errstr = imgmgr_err_str_downgrade;
                return MGMT_ERR_EBADSTATE;
            }
        }

#if MYNEWT_VAL(IMGMGR_LAZY_ERASE)
        (void) empty;
#else
        rc = flash_area_open(action->area_id, &fa);
        if (rc) {
            *errstr = imgmgr_err_str_flash_open_failed;
            return MGMT_ERR_EUNKNOWN;
        }

        rc = flash_area_is_empty(fa, &empty);
        flash_area_close(fa);
        if (rc) {
            return MGMT_ERR_EUNKNOWN;
        }

        action->erase = !empty;
#endif
    } else {
        /* Continuation of upload. */
        action->area_id = imgr_state.area_id;
        action->size = imgr_state.size;

        if (req->off != imgr_state.off) {
            /*
             * Invalid offset. Drop the data, and respond with the offset we're
             * expecting data for.
             */
            return 0;
        }
    }

    /* Calculate size of flash write. */
    action->write_bytes = req->data_len;
    if (req->off + req->data_len < action->size) {
        /*
         * Respect flash write alignment if not in the last block
         */
        rc = flash_area_open(action->area_id, &fa);
        if (rc) {
            *errstr = imgmgr_err_str_flash_open_failed;
            return MGMT_ERR_EUNKNOWN;
        }

        rem_bytes = req->data_len % flash_area_align(fa);
        flash_area_close(fa);

        if (rem_bytes) {
            action->write_bytes -= rem_bytes;
        }
    }

    action->proceed = true;
    return 0;
}

static int
imgr_upload_good_rsp(struct mgmt_cbuf *cb)
{
    CborError err = CborNoError;

    err |= cbor_encode_text_stringz(&cb->encoder, "rc");
    err |= cbor_encode_int(&cb->encoder, MGMT_ERR_EOK);
    err |= cbor_encode_text_stringz(&cb->encoder, "off");
    err |= cbor_encode_int(&cb->encoder, imgr_state.off);

    if (err != 0) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}

/**
 * Logs an upload request if necessary.
 *
 * @param is_first              Whether the request includes the first chunk of
 *                                  the image.
 * @param is_last               Whether the request includes the last chunk of
 *                                  the image.
 * @param status                The result of processing the upload request
 *                                  (MGMT_ERR code).
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
imgr_upload_log(bool is_first, bool is_last, int status)
{
    uint8_t hash[IMGMGR_HASH_LEN];
    const uint8_t *hashp;
    int rc;

    if (is_first) {
        return imgmgr_log_upload_start(status);
    }

    if (is_last || status != 0) {
        /* Log the image hash if we know it. */
        rc = imgr_read_info(1, NULL, hash, NULL);
        if (rc != 0) {
            hashp = NULL;
        } else {
            hashp = hash;
        }

        return imgmgr_log_upload_done(status, hashp);
    }

    /* Nothing to log. */
    return 0;
}

static int
imgr_upload(struct mgmt_cbuf *cb)
{
    struct imgr_upload_req req = {
        .off = -1,
        .size = -1,
        .data_len = 0,
        .data_sha_len = 0,
        .upgrade = false,
    };
    const struct cbor_attr_t off_attr[] = {
        [0] = {
            .attribute = "data",
            .type = CborAttrByteStringType,
            .addr.bytestring.data = req.img_data,
            .addr.bytestring.len = &req.data_len,
            .len = sizeof(req.img_data)
        },
        [1] = {
            .attribute = "len",
            .type = CborAttrUnsignedIntegerType,
            .addr.uinteger = &req.size,
            .nodefault = true
        },
        [2] = {
            .attribute = "off",
            .type = CborAttrUnsignedIntegerType,
            .addr.uinteger = &req.off,
            .nodefault = true
        },
        [3] = {
            .attribute = "sha",
            .type = CborAttrByteStringType,
            .addr.bytestring.data = req.data_sha,
            .addr.bytestring.len = &req.data_sha_len,
            .len = sizeof(req.data_sha)
        },
        [4] = {
            .attribute = "upgrade",
            .type = CborAttrBooleanType,
            .addr.boolean = &req.upgrade,
            .dflt.boolean = false,
        },
        [5] = { 0 },
    };
    int rc;
    const char *errstr = NULL;
    struct imgr_upload_action action;
    const struct flash_area *fa = NULL;

    rc = cbor_read_object(&cb->it, off_attr);
    if (rc != 0) {
        return MGMT_ERR_EINVAL;
    }

    /* Determine what actions to take as a result of this request. */
    rc = imgr_upload_inspect(&req, &action, &errstr);
    if (rc != 0) {
        imgmgr_dfu_stopped();
        return rc;
    }

    if (!action.proceed) {
        /* Request specifies incorrect offset.  Respond with a success code and
         * the correct offset.
         */
        return imgr_upload_good_rsp(cb);
    }

    /* Request is valid.  Give the application a chance to reject this upload
     * request.
     */
    if (imgr_upload_cb != NULL) {
        rc = imgr_upload_cb(req.off, action.size, imgr_upload_arg);
        if (rc != 0) {
            errstr = imgmgr_err_str_app_reject;
            goto end;
        }
    }

    /* Remember flash area ID and image size for subsequent upload requests. */
    imgr_state.area_id = action.area_id;
    imgr_state.size = action.size;

    rc = flash_area_open(imgr_state.area_id, &fa);
    if (rc != 0) {
        rc = MGMT_ERR_EUNKNOWN;
        errstr = imgmgr_err_str_flash_open_failed;
        goto end;
    }

    if (req.off == 0) {
        /*
         * New upload.
         */
        imgr_state.off = 0;

        imgmgr_dfu_started();

        /*
         * We accept SHA trimmed to any length by client since it's up to client
         * to make sure provided data are good enough to avoid collisions when
         * resuming upload.
         */
        imgr_state.data_sha_len = req.data_sha_len;
        memcpy(imgr_state.data_sha, req.data_sha, req.data_sha_len);
        memset(&imgr_state.data_sha[req.data_sha_len], 0,
               IMGMGR_DATA_SHA_LEN - req.data_sha_len);

#if MYNEWT_VAL(LOG_FCB_SLOT1)
        /*
         * If logging to slot1 is enabled, make sure it's locked before
         * erasing so log handler does not corrupt our data.
         */
        if (imgr_state.area_id == FLASH_AREA_IMAGE_1) {
            log_fcb_slot1_lock();
        }
#endif

#if MYNEWT_VAL(IMGMGR_LAZY_ERASE)
        /* setup for lazy sector by sector erase */
        imgr_state.sector_id = -1;
        imgr_state.sector_end = 0;
#else
        /* erase the entire req.size all at once */
        if (action.erase) {
            rc = flash_area_erase(fa, 0, req.size);
            if (rc != 0) {
                rc = MGMT_ERR_EUNKNOWN;
                errstr = imgmgr_err_str_flash_erase_failed;
                goto end;
            }
        }
#endif
    }

    /* Write the image data to flash. */
    if (req.data_len != 0) {
#if MYNEWT_VAL(IMGMGR_LAZY_ERASE)
        /* erase as we cross sector boundaries */
        if (imgr_erase_if_needed(fa, req.off, action.write_bytes) != 0) {
            rc = MGMT_ERR_EUNKNOWN;
            errstr = imgmgr_err_str_flash_erase_failed;
            goto end;
        }
#endif
        rc = flash_area_write(fa, req.off, req.img_data, action.write_bytes);
        if (rc != 0) {
            rc = MGMT_ERR_EUNKNOWN;
            errstr = imgmgr_err_str_flash_write_failed;
            goto end;
        } else {
            imgr_state.off += action.write_bytes;
            if (imgr_state.off == imgr_state.size) {
                /* Done */
                imgmgr_dfu_pending();
                imgr_state.area_id = -1;
            }
        }
    }

end:
    if (fa != NULL) {
        flash_area_close(fa);
    }

    imgr_upload_log(req.off == 0, imgr_state.off == imgr_state.size, rc);

    if (rc != 0) {
        imgmgr_dfu_stopped();
        return imgr_error_rsp(cb, rc, errstr);
    }

    return imgr_upload_good_rsp(cb);
}

void
imgmgr_dfu_stopped(void)
{
    if (imgmgr_dfu_callbacks_fn && imgmgr_dfu_callbacks_fn->dfu_stopped_cb) {
        imgmgr_dfu_callbacks_fn->dfu_stopped_cb();
    }
}

void
imgmgr_dfu_started(void)
{
    if (imgmgr_dfu_callbacks_fn && imgmgr_dfu_callbacks_fn->dfu_started_cb) {
        imgmgr_dfu_callbacks_fn->dfu_started_cb();
    }
}

void
imgmgr_dfu_pending(void)
{
    if (imgmgr_dfu_callbacks_fn && imgmgr_dfu_callbacks_fn->dfu_pending_cb) {
        imgmgr_dfu_callbacks_fn->dfu_pending_cb();
    }
}

void
imgmgr_dfu_confirmed(void)
{
    if (imgmgr_dfu_callbacks_fn && imgmgr_dfu_callbacks_fn->dfu_confirmed_cb) {
        imgmgr_dfu_callbacks_fn->dfu_confirmed_cb();
    }
}

void
imgr_set_upload_cb(imgr_upload_fn *cb, void *arg)
{
    imgr_upload_cb = cb;
    imgr_upload_arg = arg;
}

void
imgmgr_register_callbacks(const imgmgr_dfu_callbacks_t *cb_struct)
{
    imgmgr_dfu_callbacks_fn = cb_struct;
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

#if MYNEWT_VAL(LOG_FCB_SLOT1)
    /*
     * If logging to slot1 is enabled, make sure we lock it if slot1 is in use
     * to prevent data corruption.
     */
    if (imgmgr_state_slot_in_use(1)) {
        log_fcb_slot1_lock();
    }
#endif
}
