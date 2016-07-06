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
#include <os/endian.h>

#include <limits.h>
#include <assert.h>
#include <string.h>
#include <hal/hal_bsp.h>
#include <hal/flash_map.h>
#include <newtmgr/newtmgr.h>
#include <json/json.h>
#include <util/base64.h>

#include <bootutil/image.h>

#include "imgmgr/imgmgr.h"
#include "imgmgr_priv.h"

static int imgr_list(struct nmgr_jbuf *);
static int imgr_list2(struct nmgr_jbuf *);
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
    },
    [IMGMGR_NMGR_OP_BOOT] = {
        .nh_read = imgr_boot_read,
        .nh_write = imgr_boot_write
    },
    [IMGMGR_NMGR_OP_FILE] = {
#ifdef FS_PRESENT
        .nh_read = imgr_file_download,
        .nh_write = imgr_file_upload
#else
        .nh_read = imgr_noop,
        .nh_write = imgr_noop
#endif
    },
    [IMGMGR_NMGR_OP_LIST2] = {
        .nh_read = imgr_list2,
        .nh_write = imgr_noop
    },
    [IMGMGR_NMGR_OP_BOOT2] = {
        .nh_read = imgr_boot2_read,
        .nh_write = imgr_boot2_write
    },
    [IMGMGR_NMGR_OP_CORELIST] = {
#ifdef COREDUMP_PRESENT
        .nh_read = imgr_core_list,
        .nh_write = imgr_noop,
#else
        .nh_read = imgr_noop,
        .nh_write = imgr_noop
#endif
    },
    [IMGMGR_NMGR_OP_CORELOAD] = {
#ifdef COREDUMP_PRESENT
        .nh_read = imgr_core_load,
        .nh_write = imgr_core_erase,
#else
        .nh_read = imgr_noop,
        .nh_write = imgr_noop
#endif
    }
};

static struct nmgr_group imgr_nmgr_group = {
    .ng_handlers = (struct nmgr_handler *)imgr_nmgr_handlers,
    .ng_handlers_count =
    sizeof(imgr_nmgr_handlers) / sizeof(imgr_nmgr_handlers[0]),
    .ng_group_id = NMGR_GROUP_ID_IMAGE,
};

struct imgr_state imgr_state;

/*
 * Read version and build hash from image located in flash area 'area_id'.
 *
 * Returns -1 if area is not readable.
 * Returns 0 if image in slot is ok, and version string is valid.
 * Returns 1 if there is not a full image.
 * Returns 2 if slot is empty. XXXX not there yet
 */
int
imgr_read_info(int area_id, struct image_version *ver, uint8_t *hash)
{
    struct image_header *hdr;
    struct image_tlv *tlv;
    int rc = -1;
    int rc2;
    const struct flash_area *fa;
    uint8_t data[sizeof(struct image_header)];
    uint32_t data_off, data_end;

    hdr = (struct image_header *)data;
    rc2 = flash_area_open(area_id, &fa);
    if (rc2) {
        return -1;
    }
    rc2 = flash_area_read(fa, 0, hdr, sizeof(*hdr));
    if (rc2) {
        goto end;
    }
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

    /*
     * Build ID is in a TLV after the image.
     */
    data_off = hdr->ih_hdr_size + hdr->ih_img_size;
    data_end = data_off + hdr->ih_tlv_size;

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
    return imgr_read_info(bsp_imgr_current_slot(), ver, NULL);
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

    for (i = FLASH_AREA_IMAGE_0; i <= FLASH_AREA_IMAGE_1; i++) {
        if (imgr_read_info(i, &ver, hash) != 0) {
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

    for (i = FLASH_AREA_IMAGE_0; i <= FLASH_AREA_IMAGE_1; i++) {
        if (imgr_read_info(i, ver, hash) != 0) {
            continue;
        }
        if (!memcmp(hash, find, IMGMGR_HASH_LEN)) {
            return i;
        }
    }
    return -1;
}

static int
imgr_list(struct nmgr_jbuf *njb)
{
    struct image_version ver;
    int i;
    int rc;
    struct json_encoder *enc;
    struct json_value array;
    struct json_value versions[IMGMGR_MAX_IMGS];
    struct json_value *version_ptrs[IMGMGR_MAX_IMGS];
    char vers_str[IMGMGR_MAX_IMGS][IMGMGR_NMGR_MAX_VER];
    int ver_len;
    int cnt = 0;

    for (i = FLASH_AREA_IMAGE_0; i <= FLASH_AREA_IMAGE_1; i++) {
        rc = imgr_read_info(i, &ver, NULL);
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
imgr_list2(struct nmgr_jbuf *njb)
{
    struct json_encoder *enc;
    int i;
    int rc;
    struct image_version ver;
    uint8_t hash[IMGMGR_HASH_LEN]; /* SHA256 hash */
    struct json_value jv_ver;
    char vers_str[IMGMGR_NMGR_MAX_VER];
    char hash_str[IMGMGR_HASH_STR + 1];
    int ver_len;

    enc = &njb->njb_enc;

    json_encode_object_start(enc);
    json_encode_array_name(enc, "images");
    json_encode_array_start(enc);
    for (i = FLASH_AREA_IMAGE_0; i <= FLASH_AREA_IMAGE_1; i++) {
        rc = imgr_read_info(i, &ver, hash);
        if (rc != 0) {
            continue;
        }
        ver_len = imgr_ver_str(&ver, vers_str);
        base64_encode(hash, IMGMGR_HASH_LEN, hash_str, 1);
        JSON_VALUE_STRINGN(&jv_ver, vers_str, ver_len);

        json_encode_object_start(enc);
        json_encode_object_entry(enc, hash_str, &jv_ver);
        json_encode_object_finish(enc);
    }
    json_encode_array_finish(enc);
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
    long long unsigned int off = UINT_MAX;
    long long unsigned int size = UINT_MAX;
    const struct json_attr_t off_attr[4] = {
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
        },
        [2] = {
            .attribute = "len",
            .type = t_uinteger,
            .addr.uinteger = &size,
            .nodefault = true
        }
    };
    struct image_version ver;
    struct image_header *hdr;
    struct json_encoder *enc;
    struct json_value jv;
    int active;
    int best;
    int rc;
    int len;
    int i;

    rc = json_read_object(&njb->njb_buf, off_attr);
    if (rc || off == UINT_MAX) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }
    len = strlen(img_data);
    if (len) {
        len = base64_decode(img_data, img_data);
        if (len < 0) {
            rc = NMGR_ERR_EINVAL;
            goto err;
        }
    }

    if (off == 0) {
        if (len < sizeof(struct image_header)) {
            /*
             * Image header is the first thing in the image.
             */
            rc = NMGR_ERR_EINVAL;
            goto err;
        }
        hdr = (struct image_header *)img_data;
        if (hdr->ih_magic != IMAGE_MAGIC) {
            rc = NMGR_ERR_EINVAL;
            goto err;
        }

        /*
         * New upload.
         */
        imgr_state.upload.off = 0;
        imgr_state.upload.size = size;
        active = bsp_imgr_current_slot();
        best = -1;

        for (i = FLASH_AREA_IMAGE_0; i <= FLASH_AREA_IMAGE_1; i++) {
            rc = imgr_read_info(i, &ver, NULL);
            if (rc < 0) {
                continue;
            }
            if (rc == 0) {
                if (!memcmp(&ver, &hdr->ih_ver, sizeof(ver))) {
                    if (active == i) {
                        rc = NMGR_ERR_EINVAL;
                        goto err;
                    } else {
                        best = i;
                        break;
                    }
                }
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
                    best = i;
                }
                continue;
            }
            best = i;
            break;
        }
        if (best >= 0) {
            if (imgr_state.upload.fa) {
                flash_area_close(imgr_state.upload.fa);
                imgr_state.upload.fa = NULL;
            }
            rc = flash_area_open(best, &imgr_state.upload.fa);
            if (rc) {
                rc = NMGR_ERR_EINVAL;
                goto err;
            }
	    if (IMAGE_SIZE(hdr) > imgr_state.upload.fa->fa_size) {
                rc = NMGR_ERR_EINVAL;
                goto err;
            }
            /*
             * XXXX only erase if needed.
             */
            rc = flash_area_erase(imgr_state.upload.fa, 0,
              imgr_state.upload.fa->fa_size);
        } else {
            /*
             * No slot where to upload!
             */
            rc = NMGR_ERR_ENOMEM;
            goto err;
        }
    } else if (off != imgr_state.upload.off) {
        /*
         * Invalid offset. Drop the data, and respond with the offset we're
         * expecting data for.
         */
        goto out;
    }

    if (!imgr_state.upload.fa) {
        rc = NMGR_ERR_EINVAL;
        goto err;
    }
    if (len) {
        rc = flash_area_write(imgr_state.upload.fa, imgr_state.upload.off,
          img_data, len);
        if (rc) {
            rc = NMGR_ERR_EINVAL;
            goto err_close;
        }
        imgr_state.upload.off += len;
        if (imgr_state.upload.size == imgr_state.upload.off) {
            /* Done */
            flash_area_close(imgr_state.upload.fa);
            imgr_state.upload.fa = NULL;
        }
    }
out:
    enc = &njb->njb_enc;

    json_encode_object_start(enc);

    JSON_VALUE_INT(&jv, NMGR_ERR_EOK);
    json_encode_object_entry(enc, "rc", &jv);

    JSON_VALUE_UINT(&jv, imgr_state.upload.off);
    json_encode_object_entry(enc, "off", &jv);

    json_encode_object_finish(enc);

    return 0;
err_close:
    flash_area_close(imgr_state.upload.fa);
    imgr_state.upload.fa = NULL;
err:
    nmgr_jbuf_setoerr(njb, rc);
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
