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

TEST_CASE(boot_serial_img_msg)
{
    char img[16];
    char enc_img[BASE64_ENCODE_SIZE(sizeof(img)) + 1];
    char buf[sizeof(struct nmgr_hdr) + sizeof(enc_img) + 32];
    int len;
    int rc;
    struct nmgr_hdr *hdr;
    const struct flash_area *fap;

    memset(img, 0xa5, sizeof(img));
    len = base64_encode(img, sizeof(img), enc_img, 1);
    assert(len > 0);

    hdr = (struct nmgr_hdr *)buf;
    memset(hdr, 0, sizeof(*hdr));
    hdr->nh_op = NMGR_OP_WRITE;
    hdr->nh_group = htons(NMGR_GROUP_ID_IMAGE);
    hdr->nh_id = IMGMGR_NMGR_OP_UPLOAD;

    len = sprintf((char *)(hdr + 1),
                  "{\"off\":0,\"len\":16,\"data\":\"%s\"}", enc_img);
    hdr->nh_len = htons(len);

    len = sizeof(*hdr) + len;

    tx_msg(buf, len);

    /*
     * Validate contents inside image 0 slot
     */
    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fap);
    assert(rc == 0);

    rc = flash_area_read(fap, 0, enc_img, sizeof(img));
    assert(rc == 0);
    assert(!memcmp(enc_img, img, sizeof(img)));
}
