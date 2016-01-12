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
#include <util/flash_map.h>
#include <newtmgr/newtmgr.h>

#include <bootutil/image.h>

#include "imgmgr/imgmgr.h"
#include "imgmgr_priv.h"

static int imgr_list(struct nmgr_hdr *nmr, struct os_mbuf *req,
  uint16_t srcoff, struct nmgr_hdr *rsp_hdr, struct os_mbuf *rsp);
static int imgr_noop(struct nmgr_hdr *nmr, struct os_mbuf *req,
  uint16_t srcoff, struct nmgr_hdr *rsp_hdr, struct os_mbuf *rsp);
static int imgr_upload(struct nmgr_hdr *nmr, struct os_mbuf *req,
  uint16_t srcoff, struct nmgr_hdr *rsp_hdr, struct os_mbuf *rsp);

static const struct nmgr_handler imgr_nmgr_handlers[] = {
    [IMGMGR_NMGR_OP_LIST] = {
        .nh_read = imgr_list,
        .nh_write = imgr_list
    },
    [IMGMGR_NMGR_OP_UPLOAD] = {
        .nh_read = imgr_noop,
        .nh_write = imgr_upload
    },
    [IMGMGR_NMGR_OP_BOOT] = {
        .nh_read = imgr_boot_read,
        .nh_write = imgr_boot_write
    }
};

static struct nmgr_group imgr_nmgr_group = {
    .ng_handlers = (struct nmgr_handler *)imgr_nmgr_handlers,
    .ng_handlers_count = 2,
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

int
imgr_nmgr_append_ver(struct nmgr_hdr *rsp_hdr, struct os_mbuf *rsp,
  struct image_version *to_write)
{
    struct image_version ver;

    ver = *to_write;
    ver.iv_revision = htons(ver.iv_revision);
    ver.iv_build_num = htonl(ver.iv_build_num);

    return (nmgr_rsp_extend(rsp_hdr, rsp, &ver, sizeof(ver)));
}

static int
imgr_list(struct nmgr_hdr *nmr, struct os_mbuf *req, uint16_t srcoff,
  struct nmgr_hdr *rsp_hdr, struct os_mbuf *rsp)
{
    struct image_version ver;
    int rc;
    int i;

    for (i = FLASH_AREA_IMAGE_0; i <= FLASH_AREA_IMAGE_1; i++) {
        rc = imgr_read_ver(i, &ver);
        if (rc < 0) {
            continue;
        }
        rc = imgr_nmgr_append_ver(rsp_hdr, rsp, &ver);
        if (rc) {
            goto err;
        }
    }
    return 0;
err:
    return rc;
}

static int
imgr_noop(struct nmgr_hdr *nmr, struct os_mbuf *req, uint16_t srcoff,
  struct nmgr_hdr *rsp_hdr, struct os_mbuf *rsp)
{
    return 0;
}

static int
imgr_upload(struct nmgr_hdr *nmr, struct os_mbuf *req, uint16_t srcoff,
  struct nmgr_hdr *rsp_hdr, struct os_mbuf *rsp)
{
    char img_data[IMGMGR_NMGR_MAX_MSG];
    struct imgmgr_upload_cmd *iuc;
    struct image_version ver;
    int active;
    int best;
    int rc;
    int len;
    int i;

    len = min(nmr->nh_len, sizeof(img_data));

    rc = os_mbuf_copydata(req, srcoff + sizeof(*nmr), len, img_data);
    if (rc || len < sizeof(*iuc)) {
        return OS_EINVAL;
    }
    len -= sizeof(*iuc);

    iuc = (struct imgmgr_upload_cmd *)img_data;
    iuc->iuc_off = ntohl(iuc->iuc_off);
    if (iuc->iuc_off == 0) {
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
    } else if (iuc->iuc_off != img_state.upload.off) {
        /*
         * Invalid offset. Drop the data, and respond with the offset we're
         * expecting data for.
         */
        rc = 0;
        goto out;
    }
    rc = flash_area_write(img_state.upload.fa, img_state.upload.off,
      iuc + 1, len);
    assert(rc == 0);
    img_state.upload.off += len;

out:
    iuc->iuc_off = htonl(img_state.upload.off);
    rc = nmgr_rsp_extend(rsp_hdr, rsp, img_data, sizeof(*iuc));
    if (rc != 0) {
        return OS_ENOMEM;
    }
    return rc;
}

int
imgmgr_module_init(void)
{
    int rc;

    rc = nmgr_group_register(&imgr_nmgr_group);
    assert(rc == 0);
    return rc;
}
