/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <os/endian.h>
#include <bsp/bsp.h>

#include <limits.h>
#include <assert.h>
#include <string.h>
#include <hal/flash_map.h>
#include <newtmgr/newtmgr.h>
#include <json/json.h>
#include <util/base64.h>

#include <bootutil/image.h>

#include "imgmgr/imgmgr.h"
#include "imgmgr_priv.h"

static int imgr_list(struct nmgr_jbuf *);
static int imgr_noop(struct nmgr_jbuf *);
static int imgr_upload(struct nmgr_jbuf *);

static const struct nmgr_handler imgr_nmgr_handlers[] = {
    [IMGMGR_NMGR_OP_LIST] = {
        .nh_read = imgr_list,
        .nh_write = imgr_noop
    },
    [IMGMGR_NMGR_OP_UPLOAD] = {
        .nh_read = imgr_noop,
        .nh_write = imgr_upload
    }
#ifdef FS_PRESENT
    ,
    [IMGMGR_NMGR_OP_BOOT] = {
        .nh_read = imgr_boot_read,
        .nh_write = imgr_boot_write
    }
#endif
};

static struct nmgr_group imgr_nmgr_group = {
    .ng_handlers = (struct nmgr_handler *)imgr_nmgr_handlers,
#ifndef FS_PRESENT
    .ng_handlers_count = 2,
#else
    .ng_handlers_count = 3,
#endif
    .ng_group_id = NMGR_GROUP_ID_IMAGE,
};

static struct {
    struct {
        uint32_t off;
        const struct flash_area *fa;
    } upload;
} img_state;

/*
 * Read version from image header from flash area 'area_id'.
 * Returns -1 if area is not readable.
 * Returns 0 if image in slot is ok, and version string is valid.
 * Returns 1 if there is not a full image.
 * Returns 2 if slot is empty.
 */
int
imgr_read_ver(int area_id, struct image_version *ver)
{
    struct image_header hdr;
    int rc;
    const struct flash_area *fa;

    rc = flash_area_open(area_id, &fa);
    if (rc) {
        return -1;
    }
    rc = flash_area_read(fa, 0, &hdr, sizeof(hdr));
    if (rc) {
        return -1;
    }
    memset(ver, 0xff, sizeof(*ver));
    if (hdr.ih_magic == 0x96f3b83c) {
        memcpy(ver, &hdr.ih_ver, sizeof(*ver));
        rc = 0;
    } else {
        rc = 1;
    }
    flash_area_close(fa);
    return rc;
}

static int
imgr_list(struct nmgr_jbuf *njb)
{
    struct image_version ver;
    int i;
    int rc;
    struct json_encoder *enc;
    struct json_value array;
    struct json_value versions[4];
    struct json_value *version_ptrs[4];
    char vers_str[4][IMGMGR_NMGR_MAX_VER];
    int ver_len;
    int cnt = 0;

    for (i = FLASH_AREA_IMAGE_0; i <= FLASH_AREA_IMAGE_1; i++) {
        rc = imgr_read_ver(i, &ver);
        if (rc != 0) {
            continue;
        }
        ver_len = imgr_ver_str(&ver, vers_str[cnt]);
        JSON_VALUE_STRINGN(&versions[cnt], vers_str[cnt], ver_len);
        version_ptrs[cnt] = &versions[cnt];
        cnt++;
    }
    array.jv_type = JSON_VALUE_TYPE_ARRAY;
    array.jv_len = cnt;
    array.jv_val.composite.values = version_ptrs;

    enc = &njb->njb_enc;

    json_encode_object_start(enc);
    json_encode_object_entry(enc, "images", &array);
    json_encode_object_finish(enc);

    return 0;
}

static int
imgr_noop(struct nmgr_jbuf *njb)
{
    return 0;
}

static int
imgr_upload(struct nmgr_jbuf *njb)
{
    char img_data[BASE64_ENCODE_SIZE(IMGMGR_NMGR_MAX_MSG)];
    unsigned int off = UINT_MAX;
    const struct json_attr_t off_attr[3] = {
        [0] = {
            .attribute = "off",
            .type = t_uinteger,
            .addr.uinteger = &off,
            .nodefault = true
        },
        [1] = {
            .attribute = "data",
            .type = t_string,
            .addr.string = img_data,
            .len = sizeof(img_data)
        }
    };
    struct image_version ver;
    struct json_encoder *enc;
    struct json_value jv;
    int active;
    int best;
    int rc;
    int len;
    int i;

    rc = json_read_object(&njb->njb_buf, off_attr);
    if (rc || off == UINT_MAX) {
        return OS_EINVAL;
    }
    len = strlen(img_data);
    if (len) {
        len = base64_decode(img_data, img_data);
        if (len < 0) {
            return OS_EINVAL;
        }
    }

    if (off == 0) {
        /*
         * New upload.
         */
        img_state.upload.off = 0;
        active = bsp_imgr_current_slot();
        best = -1;

        for (i = FLASH_AREA_IMAGE_0; i <= FLASH_AREA_IMAGE_1; i++) {
            rc = imgr_read_ver(i, &ver);
            if (rc < 0) {
                continue;
            }
            if (rc == 0) {
                /*
                 * Image in slot is ok.
                 */
                if (active == i) {
                    /*
                     * Slot is currently active one. Can't upload to this.
                     */
                    continue;
                } else {
                    /*
                     * Not active slot, but image is ok. Use it if there are
                     * no better candidates.
                     */
                    /*
                     * XXX reject if trying to upload image which is present
                     * already.
                     */
                    best = i;
                }
                continue;
            }
            break;
        }
        if (i <= FLASH_AREA_IMAGE_1) {
            best = i;
        }
        if (best >= 0) {
            rc = flash_area_open(best, &img_state.upload.fa);
            assert(rc == 0);
            /*
             * XXXX only erase if needed.
             */
            rc = flash_area_erase(img_state.upload.fa, 0,
              img_state.upload.fa->fa_size);
        } else {
            /*
             * No slot where to upload!
             */
            assert(0);
            goto out;
        }
    } else if (off != img_state.upload.off) {
        /*
         * Invalid offset. Drop the data, and respond with the offset we're
         * expecting data for.
         */
        rc = 0;
        goto out;
    }

    if (len) {
        rc = flash_area_write(img_state.upload.fa, img_state.upload.off,
          img_data, len);
        assert(rc == 0);
        img_state.upload.off += len;
    }
out:
    enc = &njb->njb_enc;

    json_encode_object_start(enc);

    JSON_VALUE_UINT(&jv, img_state.upload.off);
    json_encode_object_entry(enc, "off", &jv);
    json_encode_object_finish(enc);

    return 0;
}

int
imgmgr_module_init(void)
{
    int rc;

    rc = nmgr_group_register(&imgr_nmgr_group);
    assert(rc == 0);
    return rc;
}
