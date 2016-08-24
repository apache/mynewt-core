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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <util/base64.h>
#include <util/crc16.h>
#include <os/endian.h>
#include <testutil/testutil.h>
#include <hal/hal_flash.h>
#include <hal/flash_map.h>

#include "../boot_serial_priv.h"

void
tx_msg(void *src, int len)
{
    char *msg;
    char *enc;
    int off;
    uint16_t crc;

    crc = htons(crc16_ccitt(CRC16_INITIAL_CRC, src, len));

    /*
     * Lazy, malloc a buffer, fill it and send it.
     */
    msg = malloc(len + 2 * sizeof(uint16_t));
    assert(msg);

    *(uint16_t *)msg = ntohs(len + sizeof(uint16_t));
    off = sizeof(uint16_t);
    memcpy(&msg[off], src, len);
    off += len;
    memcpy(&msg[off], &crc, sizeof(crc));
    off += sizeof(uint16_t);

    enc = malloc(BASE64_ENCODE_SIZE(off) + 1);
    assert(enc);

    off = base64_encode(msg, off, enc, 1);
    assert(off > 0);

    boot_serial_input(enc, off + 1);

    free(enc);
    free(msg);
}

TEST_CASE(boot_serial_setup)
{

}

TEST_CASE(boot_serial_empty_msg)
{
    char buf[4];
    struct nmgr_hdr hdr;

    boot_serial_input(buf, 0);

    tx_msg(buf, 0);

    strcpy(buf, "--");
    tx_msg(buf, 2);

    memset(&hdr, 0, sizeof(hdr));
    tx_msg(&hdr, sizeof(hdr));

    hdr.nh_op = NMGR_OP_WRITE;

    tx_msg(&hdr, sizeof(hdr));
}

TEST_CASE(boot_serial_empty_img_msg)
{
    char buf[sizeof(struct nmgr_hdr) + 32];
    struct nmgr_hdr *hdr;

    hdr = (struct nmgr_hdr *)buf;
    memset(hdr, 0, sizeof(*hdr));
    hdr->nh_op = NMGR_OP_WRITE;
    hdr->nh_group = htons(NMGR_GROUP_ID_IMAGE);
    hdr->nh_id = IMGMGR_NMGR_OP_UPLOAD;
    hdr->nh_len = htons(2);
    strcpy((char *)(hdr + 1), "{}");

    tx_msg(buf, sizeof(*hdr) + 2);
}

TEST_CASE(boot_serial_img_msg)
{
    char img[16];
    char enc_img[BASE64_ENCODE_SIZE(sizeof(img))];
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

    len = sprintf((char *)(hdr + 1), "{\"off\":0,\"len\":16,\"data\":\"%s\"}",
      enc_img);
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
        hdr->nh_group = htons(NMGR_GROUP_ID_IMAGE);
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

TEST_SUITE(boot_serial_suite)
{
    boot_serial_setup();
    boot_serial_empty_msg();
    boot_serial_empty_img_msg();
    boot_serial_img_msg();
    boot_serial_upload_bigger_image();
}

int
boot_serial_test(void)
{
    boot_serial_suite();
    return tu_any_failed;
}

#ifdef MYNEWT_SELFTEST
int
main(void)
{
    tu_config.tc_print_results = 1;
    tu_init();

    boot_serial_test();

    return tu_any_failed;
}

#endif
