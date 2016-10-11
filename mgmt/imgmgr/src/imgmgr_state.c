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

static uint8_t
imgmgr_state_flags(int query_slot)
{
    boot_split_mode_t split_mode;
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
    split_mode = boot_split_mode_get();
    switch (split_mode) {
    case BOOT_SPLIT_MODE_LOADER:
        break;

    case BOOT_SPLIT_MODE_APP:
        if (query_slot == 1) {
            flags |= IMGMGR_STATE_F_CONFIRMED;
        }
        break;

    case BOOT_SPLIT_MODE_TEST_LOADER:
        if (query_slot == 0) {
            flags |= IMGMGR_STATE_F_PENDING;
        }
        break;

    case BOOT_SPLIT_MODE_TEST_APP:
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

int
imgr_state_read(struct mgmt_jbuf *njb)
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
    uint8_t state_flags;

    enc = &njb->mjb_enc;

    json_encode_object_start(enc);
    json_encode_array_name(enc, "images");
    json_encode_array_start(enc);
    for (i = 0; i < 2; i++) {
        rc = imgr_read_info(i, &ver, hash, &flags);
        if (rc != 0) {
            continue;
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
        json_encode_object_entry(enc, "test-pending", &jv);

        JSON_VALUE_BOOL(&jv, state_flags & IMGMGR_STATE_F_CONFIRMED);
        json_encode_object_entry(enc, "confirmed", &jv);

        JSON_VALUE_BOOL(&jv, state_flags & IMGMGR_STATE_F_ACTIVE);
        json_encode_object_entry(enc, "active", &jv);

        json_encode_object_finish(enc);
    }
    json_encode_array_finish(enc);

    JSON_VALUE_INT(&jv, split_check_status());
    json_encode_object_entry(enc, "splitStatus", &jv);

    json_encode_object_finish(enc);

    return 0;
}
