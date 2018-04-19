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

#include <assert.h>

#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "cborattr/cborattr.h"
#include "tinycbor/cbor.h"
#include "split/split.h"
#include "mgmt/mgmt.h"
#include "imgmgr/imgmgr.h"
#include "imgmgr_priv.h"
#include "log/log_fcb_slot1.h"

uint8_t
imgmgr_state_flags(int query_slot)
{
    split_mode_t split_mode;
    uint8_t flags;
    int swap_type;

    assert(query_slot == 0 || query_slot == 1);

    flags = 0;

    /* Determine if this is is pending or confirmed (only applicable for
     * unified images and loaders.
     */
    swap_type = boot_swap_type();
    switch (swap_type) {
    case BOOT_SWAP_TYPE_NONE:
        if (query_slot == 0) {
            flags |= IMGMGR_STATE_F_CONFIRMED;
            flags |= IMGMGR_STATE_F_ACTIVE;
        }
        break;

    case BOOT_SWAP_TYPE_TEST:
        if (query_slot == 0) {
            flags |= IMGMGR_STATE_F_CONFIRMED;
        } else if (query_slot == 1) {
            flags |= IMGMGR_STATE_F_PENDING;
        }
        break;

    case BOOT_SWAP_TYPE_PERM:
        if (query_slot == 0) {
            flags |= IMGMGR_STATE_F_CONFIRMED;
        } else if (query_slot == 1) {
            flags |= IMGMGR_STATE_F_PENDING | IMGMGR_STATE_F_PERMANENT;
        }
        break;

    case BOOT_SWAP_TYPE_REVERT:
        if (query_slot == 0) {
            flags |= IMGMGR_STATE_F_ACTIVE;
        } else if (query_slot == 1) {
            flags |= IMGMGR_STATE_F_CONFIRMED;
        }
        break;
    }

    /* Slot 0 is always active.  Slot 1 is also active if a split app is
     * currently running.
     */
    /* XXX: The slot 0 assumption only holds when running from flash. */
    if (query_slot == 0 || split_app_active_get()) {
        flags |= IMGMGR_STATE_F_ACTIVE;
    }

    /* Read the split/status config state to determine any pending split-image
     * state changes.
     */
    split_mode = split_mode_get();
    switch (split_mode) {
    case SPLIT_MODE_LOADER:
        break;

    case SPLIT_MODE_APP:
        if (query_slot == 1) {
            flags |= IMGMGR_STATE_F_CONFIRMED;
        }
        break;

    case SPLIT_MODE_TEST_LOADER:
        if (query_slot == 0) {
            flags |= IMGMGR_STATE_F_PENDING;
        }
        break;

    case SPLIT_MODE_TEST_APP:
        if (query_slot == 1) {
            flags |= IMGMGR_STATE_F_PENDING;
        }
        break;

    default:
        assert(0);
        break;
    }

    return flags;
}

static int
imgmgr_state_any_pending(void)
{
    return (imgmgr_state_flags(0) & IMGMGR_STATE_F_PENDING) ||
           (imgmgr_state_flags(1) & IMGMGR_STATE_F_PENDING);
}

int
imgmgr_state_slot_in_use(int slot)
{
    uint8_t state_flags;

    state_flags = imgmgr_state_flags(slot);
    return (state_flags & IMGMGR_STATE_F_ACTIVE)       ||
           (state_flags & IMGMGR_STATE_F_CONFIRMED)    ||
           (state_flags & IMGMGR_STATE_F_PENDING);
}

int
imgmgr_state_set_pending(int slot, int permanent)
{
    uint32_t image_flags;
    uint8_t state_flags;
    int split_app_active;
    int rc;

    state_flags = imgmgr_state_flags(slot);
    split_app_active = split_app_active_get();

    /* Unconfirmed slots are always runable.  A confirmed slot can only be
     * run if it is a loader in a split image setup.
     */
    if ((state_flags & IMGMGR_STATE_F_CONFIRMED) &&
        (slot != 0 || !split_app_active)) {

        return MGMT_ERR_EBADSTATE;
    }

    rc = imgr_read_info(slot, NULL, NULL, &image_flags);
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
imgmgr_state_confirm(void)
{
    int rc;

    /* Confirm disallowed if a test is pending. */
    if (imgmgr_state_any_pending()) {
        return MGMT_ERR_EBADSTATE;
    }

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

#if MYNEWT_VAL(LOG_FCB_SLOT1)
        /* If logging to slot1 is enabled, we can unlock slot1 here. */
        log_fcb_slot1_unlock();
#endif
    }

    return 0;
}

int
imgmgr_state_read(struct mgmt_cbuf *cb)
{
    int i;
    int rc;
    uint32_t flags;
    struct image_version ver;
    uint8_t hash[IMGMGR_HASH_LEN]; /* SHA256 hash */
    char vers_str[IMGMGR_NMGR_MAX_VER];
    int any_non_bootable;
    int split_status;
    uint8_t state_flags;
    CborError g_err = CborNoError;
    CborEncoder images;
    CborEncoder image;

    any_non_bootable = 0;

    g_err |= cbor_encode_text_stringz(&cb->encoder, "images");

    g_err |= cbor_encoder_create_array(&cb->encoder, &images,
                                       CborIndefiniteLength);
    for (i = 0; i < 2; i++) {
        rc = imgr_read_info(i, &ver, hash, &flags);
        if (rc != 0) {
            continue;
        }

        if (flags & IMAGE_F_NON_BOOTABLE) {
            any_non_bootable = 1;
        }

        state_flags = imgmgr_state_flags(i);

        g_err |= cbor_encoder_create_map(&images, &image,
                                         CborIndefiniteLength);
        g_err |= cbor_encode_text_stringz(&image, "slot");
        g_err |= cbor_encode_int(&image, i);

        g_err |= cbor_encode_text_stringz(&image, "version");
        imgr_ver_str(&ver, vers_str);
        g_err |= cbor_encode_text_stringz(&image, vers_str);

        g_err |= cbor_encode_text_stringz(&image, "hash");
        g_err |= cbor_encode_byte_string(&image, hash, IMGMGR_HASH_LEN);

        g_err |= cbor_encode_text_stringz(&image, "bootable");
        g_err |= cbor_encode_boolean(&image, !(flags & IMAGE_F_NON_BOOTABLE));

        g_err |= cbor_encode_text_stringz(&image, "pending");
        g_err |= cbor_encode_boolean(&image,
                                     state_flags & IMGMGR_STATE_F_PENDING);

        g_err |= cbor_encode_text_stringz(&image, "confirmed");
        g_err |= cbor_encode_boolean(&image,
                                     state_flags & IMGMGR_STATE_F_CONFIRMED);

        g_err |= cbor_encode_text_stringz(&image, "active");
        g_err |= cbor_encode_boolean(&image,
                                     state_flags & IMGMGR_STATE_F_ACTIVE);

        g_err |= cbor_encode_text_stringz(&image, "permanent");
        g_err |= cbor_encode_boolean(&image,
                                     state_flags & IMGMGR_STATE_F_PERMANENT);

        g_err |= cbor_encoder_close_container(&images, &image);
    }

    g_err |= cbor_encoder_close_container(&cb->encoder, &images);

    if (any_non_bootable) {
        split_status = split_check_status();
    } else {
        split_status = SPLIT_STATUS_INVALID;
    }

    g_err |= cbor_encode_text_stringz(&cb->encoder, "splitStatus");
    g_err |= cbor_encode_int(&cb->encoder, split_status);

    if (g_err) {
        return MGMT_ERR_ENOMEM;
    }
    return 0;
}

int
imgmgr_state_write(struct mgmt_cbuf *cb)
{
    uint8_t hash[IMGMGR_HASH_LEN];
    size_t hash_len = 0;
    bool confirm;
    int slot;
    int rc;

    const struct cbor_attr_t write_attr[] = {
        [0] = {
            .attribute = "hash",
            .type = CborAttrByteStringType,
            .addr.bytestring.data = hash,
            .addr.bytestring.len = &hash_len,
            .len = sizeof(hash),
        },
        [1] = {
            .attribute = "confirm",
            .type = CborAttrBooleanType,
            .addr.boolean = &confirm,
            .dflt.boolean = false,
        },
        [2] = { 0 },
    };

    rc = cbor_read_object(&cb->it, write_attr);
    if (rc != 0) {
        return MGMT_ERR_EINVAL;
    }

    /* Determine which slot is being operated on. */
    if (hash_len == 0) {
        if (confirm) {
            slot = 0;
        } else {
            /* A 'test' without a hash is invalid. */
            return MGMT_ERR_EINVAL;
        }
    } else {
        slot = imgr_find_by_hash(hash, NULL);
        if (slot < 0) {
            return MGMT_ERR_EINVAL;
        }
    }

    if (slot == 0 && confirm) {
        /* Confirm current setup. */
        rc = imgmgr_state_confirm();
    } else {
        rc = imgmgr_state_set_pending(slot, confirm);
    }
    if (rc != 0) {
        return rc;
    }

    /* Send the current image state in the response. */
    rc = imgmgr_state_read(cb);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
