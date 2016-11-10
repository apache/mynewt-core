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
#include "boot_test.h"

TEST_CASE(boot_serial_upload_bigger_image)
{
    char img[256];
    char enc_img[64];
    char buf[sizeof(struct nmgr_hdr) + 128];
    int len;
    int off;
    int rc;
    struct nmgr_hdr *hdr;
    const struct flash_area *fap;
    int i;

    for (i = 0; i < sizeof(img); i++) {
        img[i] = i;
    }

    for (off = 0; off < sizeof(img); off += 32) {
        len = base64_encode(&img[off], 32, enc_img, 1);
        assert(len > 0);

        hdr = (struct nmgr_hdr *)buf;
        memset(hdr, 0, sizeof(*hdr));
        hdr->nh_op = NMGR_OP_WRITE;
        hdr->nh_group = htons(MGMT_GROUP_ID_IMAGE);
        hdr->nh_id = IMGMGR_NMGR_OP_UPLOAD;

        if (off) {
            len = sprintf((char *)(hdr + 1), "{\"off\":%d,\"data\":\"%s\"}",
              off, enc_img);
        } else {
            len = sprintf((char *)(hdr + 1), "{\"off\": 0 ,\"len\":%ld, "
              "\"data\":\"%s\"}", (long)sizeof(img), enc_img);
        }
        hdr->nh_len = htons(len);

        len = sizeof(*hdr) + len;

        tx_msg(buf, len);
    }

    /*
     * Validate contents inside image 0 slot
     */
    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fap);
    assert(rc == 0);

    for (off = 0; off < sizeof(img); off += sizeof(enc_img)) {
        rc = flash_area_read(fap, off, enc_img, sizeof(enc_img));
        assert(rc == 0);
        assert(!memcmp(enc_img, &img[off], sizeof(enc_img)));
    }
}
