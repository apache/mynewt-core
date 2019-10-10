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
#include "imgmgr/imgmgr.h"
#include "mgmt/mgmt.h"
#include "img_mgmt/img_mgmt.h"
#include "imgmgr_priv.h"

static imgr_upload_fn *imgr_upload_cb;
static void *imgr_upload_arg;

void
imgr_set_upload_cb(imgr_upload_fn *cb, void *arg)
{
    imgr_upload_cb = cb;
    imgr_upload_arg = arg;
}


void imgmgr_dfu_stopped(void)
{
    img_mgmt_dfu_stopped();
}

void imgmgr_dfu_started(void)
{
    img_mgmt_dfu_started();
}

void imgmgr_dfu_pending(void)
{
    img_mgmt_dfu_pending();
}

void imgmgr_dfu_confirmed(void)
{
    img_mgmt_dfu_confirmed();
}

void
imgmgr_register_callbacks(const imgmgr_dfu_callbacks_t *cb_struct)
{
    img_mgmt_register_callbacks((const img_mgmt_dfu_callbacks_t *)cb_struct);
}

uint8_t
imgmgr_state_flags(int query_slot)
{
    return img_mgmt_state_flags(query_slot);
}

int
imgr_read_info(int image_slot, struct image_version *ver, uint8_t *hash,
               uint32_t *flags)
{
    return img_mgmt_read_info(image_slot, ver, hash, flags);
}

static int
imgr_erase_state(struct mgmt_ctxt *ctxt);

static const struct mgmt_handler imgr_mgmt_handlers[] = {
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
    sizeof(imgr_mgmt_handlers) / sizeof(imgr_mgmt_handlers[0])

static struct mgmt_group imgr_mgmt_group = {
    .mg_handlers = (struct mgmt_handler *)imgr_mgmt_handlers,
    .mg_handlers_count = IMGR_HANDLER_CNT,
    .mg_group_id = MGMT_GROUP_ID_IMAGE,
};

/*
 * Read the current running image's build hash
 *
 * @param hash Ptr to hash to be filled up
 * @param hashlen Length of hash to return
 *
 * Returns -2 if either of the argument is 0 or NULL
 * Returns -1 if area is not readable
 * Returns 0 if image in slot is ok
 * Returns 1 if there is not a full image
 * Returns 2 if slot is empty
 */
int
imgr_get_current_hash(uint8_t *hash, uint16_t hashlen)
{
    uint8_t imghash[IMGMGR_HASH_LEN];
    int rc;

    if (!hashlen || !hash) {
        return -2;
    }

    rc = img_mgmt_read_info(0, NULL, imghash, NULL);
    if (rc) {
        return rc;
    }
 
    memcpy(hash, imghash, hashlen);
 
    return 0;
}

int
imgr_my_version(struct image_version *ver)
{
    return img_mgmt_read_info(boot_current_slot, ver, NULL, NULL);
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
        if (img_mgmt_read_info(i, &ver, hash, NULL) != 0) {
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
        if (img_mgmt_read_info(i, ver, hash, NULL) != 0) {
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

static int
imgr_erase_state(struct mgmt_ctxt *ctxt)
{
    const struct flash_area *fa;
    int area_id;
    int rc;
    CborError g_err = CborNoError;

    area_id = imgmgr_find_best_area_id();
    if (area_id >= 0) {
        rc = flash_area_open(area_id, &fa);
        if (rc) {
            return img_mgmt_error_rsp(ctxt, MGMT_ERR_EINVAL,
                                      img_mgmt_err_str_flash_open_failed);
        }

        rc = flash_area_erase(fa, 0, sizeof(struct image_header));
        if (rc) {
            return img_mgmt_error_rsp(ctxt, MGMT_ERR_EINVAL,
                                      img_mgmt_err_str_flash_erase_failed);
        }

        flash_area_close(fa);

    } else {
        return img_mgmt_error_rsp(ctxt, MGMT_ERR_ENOMEM, img_mgmt_err_str_no_slot);
    }

    g_err |= cbor_encode_text_stringz(&ctxt->encoder, "rc");
    g_err |= cbor_encode_int(&ctxt->encoder, MGMT_ERR_EOK);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }

    return 0;
}

void
imgmgr_module_init(void)
{
    int rc;
    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    mgmt_register_group(&imgr_mgmt_group);

#if MYNEWT_VAL(IMGMGR_CLI)
    rc = imgr_cli_register();
    SYSINIT_PANIC_ASSERT(rc == 0);
#else
    (void) rc;
#endif
}
