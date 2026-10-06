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

#include "sysinit/sysinit.h"
#include "mgmt/mgmt.h"
#include "img_mgmt/img_mgmt_impl.h"
#include "img_mgmt/img_mgmt.h"
#include "split/split.h"
#include "flash_map/flash_map.h"
#include "sysflash/sysflash.h"
#include "img_mgmt/image.h"

static int
img_mgmt_find_best_area_id(void)
{
    struct image_version ver;
    int best = -1;
    int i;
    int rc;

    for (i = 0; i < 2; i++) {
        rc = img_mgmt_read_info(i, &ver, NULL, NULL);
        if (rc < 0) {
            continue;
        }
        if (rc == 0) {
            /* Image in slot is ok. */
            if (img_mgmt_slot_in_use(i)) {
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
img_mgmt_vercmp(const struct image_version *a, const struct image_version *b)
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
int
img_mgmt_impl_upload_inspect(const struct img_mgmt_upload_req *req,
                             struct img_mgmt_upload_action *action,
                             const char **errstr)
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
        *errstr = img_mgmt_err_str_hdr_malformed;
        return MGMT_ERR_EINVAL;
    }

    if (req->data_len > sizeof(req->img_data)) {
        return MGMT_ERR_EINVAL;
    }

    if (req->off == 0) {
        /* First upload chunk. */
        if (req->data_len < sizeof(struct image_header)) {
            /*
             * Image header is the first thing in the image.
             */
            *errstr = img_mgmt_err_str_hdr_malformed;
            return MGMT_ERR_EINVAL;
        }

        if (req->size == -1) {
            /* Request did not include a `len` field. */
            *errstr = img_mgmt_err_str_hdr_malformed;
            return MGMT_ERR_EINVAL;
        }
        action->size = req->size;

        hdr = (struct image_header *)req->img_data;
        if (hdr->ih_magic != IMAGE_MAGIC) {
            *errstr = img_mgmt_err_str_magic_mismatch;
            return MGMT_ERR_EINVAL;
        }

        if (req->data_sha_len > IMG_MGMT_DATA_SHA_LEN) {
            return MGMT_ERR_EINVAL;
        }

        /*
         * If request includes proper data hash we can check whether there is
         * upload in progress (interrupted due to e.g. link disconnection) with
         * the same data hash so we can just resume it by simply including
         * current upload offset in response.
         */
        if ((req->data_sha_len > 0) && (g_img_mgmt_state.area_id != -1)) {
            if ((g_img_mgmt_state.data_sha_len == req->data_sha_len) &&
                            !memcmp(g_img_mgmt_state.data_sha, req->data_sha,
                                                        req->data_sha_len)) {
                return 0;
            }
        }

        action->area_id = img_mgmt_find_best_area_id();
        if (action->area_id < 0) {
            /* No slot where to upload! */
            *errstr = img_mgmt_err_str_no_slot;
            return MGMT_ERR_ENOMEM;
        }

        /* Image must fit in slot */
        rc = flash_area_open(action->area_id, &fa);
        if (rc) {
            *errstr = img_mgmt_err_str_flash_open_failed;
            return MGMT_ERR_EUNKNOWN;
        }
        if (action->size > fa->fa_size) {
            flash_area_close(fa);
            *errstr = img_mgmt_err_str_no_slot;
            return MGMT_ERR_EINVAL;
        }
        flash_area_close(fa);

        if (req->upgrade) {
            /* User specified upgrade-only.  Make sure new image version is
             * greater than that of the currently running image.
             */
            rc = img_mgmt_my_version(&cur_ver);
            if (rc != 0) {
                return MGMT_ERR_EUNKNOWN;
            }

            if (img_mgmt_vercmp(&cur_ver, &hdr->ih_ver) >= 0) {
                *errstr = img_mgmt_err_str_downgrade;
                return MGMT_ERR_EBADSTATE;
            }
        }

#if MYNEWT_VAL(IMG_MGMT_LAZY_ERASE)
        (void) empty;
#else
        rc = flash_area_open(action->area_id, &fa);
        if (rc) {
            *errstr = img_mgmt_err_str_flash_open_failed;
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
        action->area_id = g_img_mgmt_state.area_id;
        action->size = g_img_mgmt_state.size;

        if (req->off != g_img_mgmt_state.off) {
            /*
             * Invalid offset. Drop the data, and respond with the offset we're
             * expecting data for.
             */
            return 0;
        }
    }

    /* Chunk must not extend past image size */
    if (req->off + req->data_len > action->size) {
        return MGMT_ERR_EINVAL;
    }

    /* Calculate size of flash write. */
    action->write_bytes = req->data_len;
    if (req->off + req->data_len < action->size) {
        /*
         * Respect flash write alignment if not in the last block
         */
        rc = flash_area_open(action->area_id, &fa);
        if (rc) {
            *errstr = img_mgmt_err_str_flash_open_failed;
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

int
img_mgmt_impl_erase_slot(void)
{
#ifndef FLASH_AREA_IMAGE_1
    return MGMT_ERR_ENOTSUP;
#else
    const struct flash_area *fa;
    bool empty;
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_1, &fa);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    rc = flash_area_is_empty(fa, &empty);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    if (!empty) {
        rc = flash_area_erase(fa, 0, fa->fa_size);
        if (rc != 0) {
            return MGMT_ERR_EUNKNOWN;
        }
    }

    return 0;
#endif
}

int
img_mgmt_impl_write_pending(int slot, bool permanent)
{
    uint32_t image_flags;
    uint8_t state_flags;
    int split_app_active;
    int rc;

    state_flags = img_mgmt_state_flags(slot);
    split_app_active = split_app_active_get();

    /* Unconfirmed slots are always runable.  A confirmed slot can only be
     * run if it is a loader in a split image setup.
     */
    if (state_flags & IMG_MGMT_STATE_F_CONFIRMED &&
        (slot != 0 || !split_app_active)) {

        return MGMT_ERR_EBADSTATE;
    }

    rc = img_mgmt_read_info(slot, NULL, NULL, &image_flags);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    if (!(image_flags & IMAGE_F_NON_BOOTABLE)) {
        /* Unified image or loader. */
        if (!split_app_active) {
            /* No change in split status. */
            rc = boot_set_pending(permanent);
            if (rc != 0) {
                return MGMT_ERR_EUNKNOWN;
            }
        } else {
            /* Currently loader + app; testing loader-only. */
            if (permanent) {
                rc = split_write_split(SPLIT_MODE_LOADER);
            } else {
                rc = split_write_split(SPLIT_MODE_TEST_LOADER);
            }
            if (rc != 0) {
                return MGMT_ERR_EUNKNOWN;
            }
        }
    } else {
        /* Testing split app. */
        if (permanent) {
            rc = split_write_split(SPLIT_MODE_APP);
        } else {
            rc = split_write_split(SPLIT_MODE_TEST_APP);
        }
        if (rc != 0) {
            return MGMT_ERR_EUNKNOWN;
        }
    }

    return 0;
}

int
img_mgmt_impl_write_confirmed(void)
{
    int rc;

    /* Confirm the unified image or loader in slot 0. */
    rc = boot_set_confirmed();
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    /* If a split app in slot 1 is active, confirm it as well. */
    if (split_app_active_get()) {
        rc = split_write_split(SPLIT_MODE_APP);
        if (rc != 0) {
            return MGMT_ERR_EUNKNOWN;
        }
    } else {
        rc = split_write_split(SPLIT_MODE_LOADER);
        if (rc != 0) {
            return MGMT_ERR_EUNKNOWN;
        }
    }

    return 0;
}

int
img_mgmt_impl_read(int slot, unsigned int offset, void *dst,
                   unsigned int num_bytes)
{
    const struct flash_area *fa;
    int area_id;
    int rc;

    area_id = flash_area_id_from_image_slot(slot);
    rc = flash_area_open(area_id, &fa);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    rc = flash_area_read(fa, offset, dst, num_bytes);
    flash_area_close(fa);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    return 0;
}

#if MYNEWT_VAL(IMG_MGMT_LAZY_ERASE)
int
img_mgmt_impl_write_image_data(unsigned int offset, const void *data,
                               unsigned int num_bytes, bool last)
{
#ifndef FLASH_AREA_IMAGE_1
    return MGMT_ERR_ENOTSUP;
#else
    const struct flash_area *fa;
    struct flash_area sector;
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_1, &fa);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    /* Check if there any unerased target sectors, if not clean them. */
    while ((fa->fa_off + offset + num_bytes) > g_img_mgmt_state.sector_end) {
        rc = flash_area_getnext_sector(fa->fa_id, &g_img_mgmt_state.sector_id,
                                       &sector);
        if (rc) {
            goto err;
        }
        rc = flash_area_erase(&sector, 0, sector.fa_size);
        if (rc) {
            goto err;
        }
        g_img_mgmt_state.sector_end = sector.fa_off + sector.fa_size;
    }

    if (last) {
        g_img_mgmt_state.sector_id = -1;
        g_img_mgmt_state.sector_end = 0;
    }

    rc = flash_area_write(fa, offset, data, num_bytes);
    flash_area_close(fa);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    return 0;

err:
    g_img_mgmt_state.sector_id = -1;
    g_img_mgmt_state.sector_end = 0;
    return MGMT_ERR_EUNKNOWN;
#endif
}

#else
int
img_mgmt_impl_write_image_data(unsigned int offset, const void *data,
                               unsigned int num_bytes, bool last)
{
#ifndef FLASH_AREA_IMAGE_1
    return MGMT_ERR_ENOTSUP;
#else
    const struct flash_area *fa;
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_1, &fa);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    rc = flash_area_write(fa, offset, data, num_bytes);
    flash_area_close(fa);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    return 0;
#endif
}
#endif

int
img_mgmt_impl_erase_image_data(unsigned int off, unsigned int num_bytes)
{
#ifndef FLASH_AREA_IMAGE_1
    return MGMT_ERR_ENOTSUP;
#else
    const struct flash_area *fa;
    int rc;

    rc = flash_area_open(FLASH_AREA_IMAGE_1, &fa);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    rc = flash_area_erase(fa, off, num_bytes);
    flash_area_close(fa);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    return 0;
#endif
}

#if MYNEWT_VAL(IMG_MGMT_LAZY_ERASE)
int
img_mgmt_impl_erase_if_needed(uint32_t off, uint32_t len)
{
#ifndef FLASH_AREA_IMAGE_1
    return MGMT_ERR_ENOTSUP;
#else
    int rc = 0;
    struct flash_area sector;
    const struct flash_area *cfa;

    rc = flash_area_open(FLASH_AREA_IMAGE_1, &cfa);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    while ((cfa->fa_off + off + len) > g_img_mgmt_state.sector_end) {
        rc = flash_area_getnext_sector(cfa->fa_id,
                                       &g_img_mgmt_state.sector_id, &sector);
        if (rc) {
            goto done;
        }
        rc = flash_area_erase(&sector, 0, sector.fa_size);
        if (rc) {
            goto done;
        }
        g_img_mgmt_state.sector_end = sector.fa_off + sector.fa_size;
    }

done:
    flash_area_close(cfa);
    return rc;
#endif
}
#endif

int
img_mgmt_impl_swap_type(int slot)
{
    assert(slot == 0 || slot == 1);

    switch (boot_swap_type()) {
    case BOOT_SWAP_TYPE_NONE:
        return IMG_MGMT_SWAP_TYPE_NONE;
    case BOOT_SWAP_TYPE_TEST:
        return IMG_MGMT_SWAP_TYPE_TEST;
    case BOOT_SWAP_TYPE_PERM:
        return IMG_MGMT_SWAP_TYPE_PERM;
    case BOOT_SWAP_TYPE_REVERT:
        return IMG_MGMT_SWAP_TYPE_REVERT;
    default:
        assert(0);
        return IMG_MGMT_SWAP_TYPE_NONE;
    }
}

int
img_mgmt_impl_erased_val(int slot, uint8_t *erased_val)
{
    const struct flash_area *fa;
    int rc;

    rc = flash_area_open(flash_area_id_from_image_slot(slot), &fa);
    if (rc != 0) {
      return MGMT_ERR_EUNKNOWN;
    }

    *erased_val = flash_area_erased_val(fa);
    flash_area_close(fa);

    return 0;
}

void
img_mgmt_module_init(void)
{
    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    img_mgmt_register_group();
}
