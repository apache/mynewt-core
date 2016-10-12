/**
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
#include "bootutil/bootutil_misc.h"
#include "base64/base64.h"
#include "split/split.h"
#include "mgmt/mgmt.h"
#include "imgmgr/imgmgr.h"
#include "imgmgr_priv.h"

#define IMGMGR_STATE_F_PENDING          0x01
#define IMGMGR_STATE_F_CONFIRMED        0x02
#define IMGMGR_STATE_F_ACTIVE           0x04

uint8_t
imgmgr_state_flags(int query_slot)
{
    split_mode_t split_mode;
    uint8_t flags;
    int slot;
    int rc;

    assert(query_slot == 0 || query_slot == 1);

    flags = 0;

    /* Determine if this is is pending or confirmed (only applicable for
     * unified images and loaders.
     */
    rc = boot_vect_read_test(&slot);
    if (rc == 0 && slot == query_slot) {
        flags |= IMGMGR_STATE_F_PENDING;
    }
    rc = boot_vect_read_main(&slot);
    if (rc == 0 && slot == query_slot) {
        flags |= IMGMGR_STATE_F_CONFIRMED;
    }

    /* Slot 0 is always active.  Slot 1 is also active if a split app is
     * currently running.
     */
    /* XXX: The slot 0 assumption only holds when running from flash. */
    if (query_slot == 0 || boot_split_app_active_get()) {
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

    /* An image cannot be both pending and confirmed. */
    assert(!(flags & IMGMGR_STATE_F_PENDING) ||
           !(flags & IMGMGR_STATE_F_CONFIRMED));

    return flags;
}

static int
imgmgr_state_any_pending(void)
{
    return imgmgr_state_flags(0) & IMGMGR_STATE_F_PENDING ||
           imgmgr_state_flags(1) & IMGMGR_STATE_F_PENDING;
}

int
imgmgr_state_slot_in_use(int slot)
{
    uint8_t state_flags;

    state_flags = imgmgr_state_flags(slot);
    return state_flags & IMGMGR_STATE_F_ACTIVE       ||
           state_flags & IMGMGR_STATE_F_CONFIRMED    ||
           state_flags & IMGMGR_STATE_F_PENDING;
}

static int
imgmgr_state_test_slot(int slot)
{
    uint32_t image_flags;
    uint8_t state_flags;
    int split_app_active;
    int rc;

    state_flags = imgmgr_state_flags(slot);
    split_app_active = boot_split_app_active_get();

    /* Unconfirmed slots are always testable.  A confirmed slot can only be
     * tested if it is a loader in a split image setup.
     */
    if (state_flags & IMGMGR_STATE_F_CONFIRMED &&
        (slot != 0 || !split_app_active)) {

        return MGMT_ERR_EINVAL;
    }

    rc = imgr_read_info(slot, NULL, NULL, &image_flags);
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    if (!(image_flags & IMAGE_F_NON_BOOTABLE)) {
        /* Unified image or loader. */
        if (!split_app_active) {
            /* No change in split status. */
            rc = boot_vect_write_test(slot);
            if (rc != 0) {
                return MGMT_ERR_EUNKNOWN;
            }
        } else {
            /* Currently loader + app; testing loader-only. */
            rc = split_write_split(SPLIT_MODE_TEST_LOADER);
        }
    } else {
        /* Testing split app. */
        rc = split_write_split(SPLIT_MODE_TEST_APP);
        if (rc != 0) {
            return MGMT_ERR_EUNKNOWN;
        }
    }

    return 0;
}

static int
imgmgr_state_confirm(void)
{
    int rc;

    /* Confirm disallowed if a test is pending. */
    if (imgmgr_state_any_pending()) {
        return MGMT_ERR_EINVAL;
    }

    /* Confirm the unified image or loader in slot 0. */
    rc = boot_vect_write_main();
    if (rc != 0) {
        return MGMT_ERR_EUNKNOWN;
    }

    /* If a split app in slot 1 is active, confirm it as well. */
    if (boot_split_app_active_get()) {
        rc = split_write_split(SPLIT_MODE_APP);
        if (rc != 0) {
            return MGMT_ERR_EUNKNOWN;
        }
    }

    return 0;
}

int
imgmgr_state_read(struct mgmt_jbuf *njb)
{
    struct json_encoder *enc;
    int i;
    int rc;
    uint32_t flags;
    struct image_version ver;
    uint8_t hash[IMGMGR_HASH_LEN]; /* SHA256 hash */
    struct json_value jv;
    char vers_str[IMGMGR_NMGR_MAX_VER];
    char hash_str[IMGMGR_HASH_STR + 1];
    int ver_len;
    int any_non_bootable;
    int split_status;
    uint8_t state_flags;

    any_non_bootable = 0;
    enc = &njb->mjb_enc;

    json_encode_object_start(enc);

    json_encode_array_name(enc, "images");
    json_encode_array_start(enc);
    for (i = 0; i < 2; i++) {
        rc = imgr_read_info(i, &ver, hash, &flags);
        if (rc != 0) {
            continue;
        }

        if (flags & IMAGE_F_NON_BOOTABLE) {
            any_non_bootable = 1;
        }

        state_flags = imgmgr_state_flags(i);

        json_encode_object_start(enc);

        JSON_VALUE_INT(&jv, i);
        json_encode_object_entry(enc, "slot", &jv);

        ver_len = imgr_ver_str(&ver, vers_str);
        JSON_VALUE_STRINGN(&jv, vers_str, ver_len);
        json_encode_object_entry(enc, "version", &jv);

        base64_encode(hash, IMGMGR_HASH_LEN, hash_str, 1);
        JSON_VALUE_STRING(&jv, hash_str);
        json_encode_object_entry(enc, "hash", &jv);

        JSON_VALUE_BOOL(&jv, !(flags & IMAGE_F_NON_BOOTABLE));
        json_encode_object_entry(enc, "bootable", &jv);

        JSON_VALUE_BOOL(&jv, state_flags & IMGMGR_STATE_F_PENDING);
        json_encode_object_entry(enc, "pending", &jv);

        JSON_VALUE_BOOL(&jv, state_flags & IMGMGR_STATE_F_CONFIRMED);
        json_encode_object_entry(enc, "confirmed", &jv);

        JSON_VALUE_BOOL(&jv, state_flags & IMGMGR_STATE_F_ACTIVE);
        json_encode_object_entry(enc, "active", &jv);

        json_encode_object_finish(enc);
    }
    json_encode_array_finish(enc);

    if (any_non_bootable) {
        split_status = split_check_status();
    } else {
        split_status = SPLIT_STATUS_INVALID;
    }
    JSON_VALUE_INT(&jv, split_status);
    json_encode_object_entry(enc, "splitStatus", &jv);

    json_encode_object_finish(enc);

    return 0;
}

int
imgmgr_state_write(struct mgmt_jbuf *njb)
{
    char hash_str[IMGMGR_HASH_STR + 1];
    uint8_t hash[IMGMGR_HASH_LEN];
    bool confirm;
    int slot;
    int rc;

    const struct json_attr_t write_attr[] = {
        [0] = {
            .attribute = "hash",
            .type = t_string,
            .addr.string = hash_str,
            .len = sizeof(hash_str),
        },
        [1] = {
            .attribute = "confirm",
            .type = t_boolean,
            .addr.boolean = &confirm,
            .dflt.boolean = false,
        },
        [2] = { 0 },
    };

    rc = json_read_object(&njb->mjb_buf, write_attr);
    if (rc != 0) {
        rc = MGMT_ERR_EINVAL;
        goto err;
    }

    /* Validate arguments. */
    if (hash_str[0] == '\0' && !confirm) {
        rc = MGMT_ERR_EINVAL;
        goto err;
    }
    if (hash_str[0] != '\0' && confirm) {
        rc = MGMT_ERR_EINVAL;
        goto err;
    }

    if (!confirm) {
        /* Test image with specified hash. */
        base64_decode(hash_str, hash);
        slot = imgr_find_by_hash(hash, NULL);
        if (slot < 0) {
            rc = MGMT_ERR_EINVAL;
            goto err;
        }

        rc = imgmgr_state_test_slot(slot);
        if (rc != 0) {
            goto err;
        }
    } else {
        /* Confirm current setup. */
        rc = imgmgr_state_confirm();
        if (rc != 0) {
            goto err;
        }
    }

    /* Send the current image state in the response. */
    rc = imgmgr_state_read(njb);
    if (rc != 0) {
        goto err;
    }

    return 0;

err:
    mgmt_jbuf_setoerr(njb, rc);

    return 0;
}
