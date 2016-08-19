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
#include <inttypes.h>
#include <ctype.h>
#include <stdio.h>

#include <bsp/bsp.h>

#include <hal/flash_map.h>
#include <hal/hal_flash.h>
#include <hal/hal_system.h>

#include <os/endian.h>
#include <os/os.h>
#include <os/os_malloc.h>

#include <console/console.h>

#include <util/base64.h>
#include <util/crc16.h>

#include <bootutil/image.h>

#include "boot_serial/boot_serial.h"
#include "boot_serial_priv.h"

#define BOOT_SERIAL_OUT_MAX	48

static uint32_t curr_off;
static uint32_t img_size;
static struct nmgr_hdr *bs_hdr;

static void boot_serial_output(char *data, int len);

/*
 * Looks for 'name' from NULL-terminated json data in buf.
 * Returns pointer to first character of value for that name.
 * Returns NULL if 'name' is not found.
 */
char *
bs_find_val(char *buf, char *name)
{
    char *ptr;

    ptr = strstr(buf, name);
    if (!ptr) {
        return NULL;
    }
    ptr += strlen(name);

    while (*ptr != '\0') {
        if (*ptr != ':' && !isspace(*ptr)) {
            break;
        }
        ++ptr;
    }
    if (*ptr == '\0') {
        ptr = NULL;
    }
    return ptr;
}

/*
 * List images.
 */
static void
bs_list(char *buf, int len)
{
    char *ptr;
    struct image_header hdr;
    uint8_t tmpbuf[64];
    const struct flash_area *fap = NULL;
    int good_img, need_comma = 0;
    int rc;
    int i;

    ptr = os_malloc(BOOT_SERIAL_OUT_MAX);
    if (!ptr) {
        return;
    }
    len = snprintf(ptr, BOOT_SERIAL_OUT_MAX, "{\"images\":[");
    for (i = FLASH_AREA_IMAGE_0; i <= FLASH_AREA_IMAGE_1; i++) {
        rc = flash_area_open(i, &fap);
        if (rc) {
            continue;
        }

        flash_area_read(fap, 0, &hdr, sizeof(hdr));

        if (hdr.ih_magic == IMAGE_MAGIC &&
          bootutil_img_validate(&hdr, fap->fa_flash_id, fap->fa_off,
            tmpbuf, sizeof(tmpbuf)) == 0) {
            good_img = 1;
        } else {
            good_img = 0;
        }
        if (good_img) {
            len += snprintf(ptr + len, BOOT_SERIAL_OUT_MAX - len,
              "%c\"%u.%u.%u.%u\"", need_comma ? ',' : ' ',
              hdr.ih_ver.iv_major, hdr.ih_ver.iv_minor, hdr.ih_ver.iv_revision,
              (unsigned int)hdr.ih_ver.iv_build_num);
        }
        flash_area_close(fap);
    }
    len += snprintf(ptr + len, BOOT_SERIAL_OUT_MAX - len, "]}");
    boot_serial_output(ptr, len);
    os_free(ptr);
}

/*
 * Image upload request.
 */
static void
bs_upload(char *buf, int len)
{
    char *ptr;
    char *data_ptr;
    uint32_t off, data_len = 0;
    const struct flash_area *fap = NULL;
    int rc;

    /*
     * should be json inside
     */
    ptr = bs_find_val(buf, "\"off\"");
    if (!ptr) {
        rc = NMGR_ERR_EINVAL;
        goto out;
    }
    off = strtoul(ptr, NULL, 10);

    if (off == 0) {
        ptr = bs_find_val(buf, "\"len\"");
        if (!ptr) {
            rc = NMGR_ERR_EINVAL;
            goto out;
        }
        data_len = strtoul(ptr, NULL, 10);

    }
    data_ptr = bs_find_val(buf, "\"data\"");
    if (!data_ptr) {
        rc = NMGR_ERR_EINVAL;
        goto out;
    }
    if (*data_ptr != '"') {
        rc = NMGR_ERR_EINVAL;
        goto out;
    }
    ++data_ptr;
    data_ptr = strsep(&data_ptr, "\"");
    if (!data_ptr) {
        rc = NMGR_ERR_EINVAL;
        goto out;
    }

    len = base64_decode(data_ptr, data_ptr);
    if (len <= 0) {
        rc = NMGR_ERR_EINVAL;
        goto out;
    }

    rc = flash_area_open(FLASH_AREA_IMAGE_0, &fap);
    if (rc) {
        rc = NMGR_ERR_EINVAL;
        goto out;
    }

    if (off == 0) {
        curr_off = 0;
        if (data_len > fap->fa_size) {
            rc = NMGR_ERR_EINVAL;
            goto out;
        }
        rc = flash_area_erase(fap, 0, fap->fa_size);
        if (rc) {
            rc = NMGR_ERR_EINVAL;
            goto out;
        }
        img_size = data_len;
    }
    if (off != curr_off) {
        rc = 0;
        goto out;
    }
    rc = flash_area_write(fap, curr_off, data_ptr, len);
    if (rc) {
        rc = NMGR_ERR_EINVAL;
        goto out;
    }
    curr_off += len;

out:
    ptr = os_malloc(BOOT_SERIAL_OUT_MAX);
    if (!ptr) {
        return;
    }
    if (rc == 0) {
        len = snprintf(ptr, BOOT_SERIAL_OUT_MAX, "{\"rc\":%d,\"off\":%u}",
          rc, (int)curr_off);
    } else {
        len = snprintf(ptr, BOOT_SERIAL_OUT_MAX, "{\"rc\":%d}", rc);
    }
    boot_serial_output(ptr, len);
    os_free(ptr);
    flash_area_close(fap);
}

/*
 * Console echo control. Send empty response, don't do anything.
 */
static void
bs_echo_ctl(char *buf, int len)
{
    boot_serial_output(NULL, 0);
}

/*
 * Reset, and (presumably) boot to newly uploaded image. Flush console
 * before restarting.
 */
static void
bs_reset(char *buf, int len)
{
    char msg[] = "{\"rc\":0}";

    boot_serial_output(msg, strlen(msg));
    os_time_delay(250);
    system_reset();
}

/*
 * Parse incoming line of input from console.
 * Expect newtmgr protocol with serial transport.
 */
void
boot_serial_input(char *buf, int len)
{
    int rc;
    uint16_t crc;
    uint16_t expected_len;
    struct nmgr_hdr *hdr;

    if (len < BASE64_ENCODE_SIZE(sizeof(uint16_t) * 2)) {
        return;
    }
    rc = base64_decode(buf, buf);
    if (rc < 0) {
        return;
    }
    len = rc;

    expected_len = ntohs(*(uint16_t *)buf);
    buf += sizeof(uint16_t);
    len -= sizeof(uint16_t);

    len = min(len, expected_len);

    crc = crc16_ccitt(CRC16_INITIAL_CRC, buf, len);
    if (crc || len <= sizeof(crc)) {
        return;
    }
    len -= sizeof(crc);
    buf[len] = '\0';

    hdr = (struct nmgr_hdr *)buf;
    if (len < sizeof(*hdr) ||
      (hdr->nh_op != NMGR_OP_READ && hdr->nh_op != NMGR_OP_WRITE) ||
      (ntohs(hdr->nh_len) < len - sizeof(*hdr))) {
        return;
    }
    bs_hdr = hdr;
    hdr->nh_group = ntohs(hdr->nh_group);

    buf += sizeof(*hdr);
    len -= sizeof(*hdr);

    /*
     * Limited support for commands.
     */
    if (hdr->nh_group == NMGR_GROUP_ID_IMAGE) {
        switch (hdr->nh_id) {
        case IMGMGR_NMGR_OP_LIST:
            bs_list(buf, len);
            break;
        case IMGMGR_NMGR_OP_UPLOAD:
            bs_upload(buf, len);
            break;
        default:
            break;
        }
    } else if (hdr->nh_group == NMGR_GROUP_ID_DEFAULT) {
        switch (hdr->nh_id) {
        case NMGR_ID_CONS_ECHO_CTRL:
            bs_echo_ctl(buf, len);
            break;
        case NMGR_ID_RESET:
            bs_reset(buf, len);
            break;
        default:
            break;
        }
    }
}

static void
boot_serial_output(char *data, int len)
{
    uint16_t crc;
    uint16_t totlen;
    char pkt_start[2] = { SHELL_NLIP_PKT_START1, SHELL_NLIP_PKT_START2 };
    char buf[BOOT_SERIAL_OUT_MAX];
    char encoded_buf[BASE64_ENCODE_SIZE(BOOT_SERIAL_OUT_MAX)];

    bs_hdr->nh_op++;
    bs_hdr->nh_len = htons(len);
    bs_hdr->nh_group = htons(bs_hdr->nh_group);

    crc = crc16_ccitt(CRC16_INITIAL_CRC, bs_hdr, sizeof(*bs_hdr));
    crc = crc16_ccitt(crc, data, len);
    crc = htons(crc);

    console_write(pkt_start, sizeof(pkt_start));

    totlen = len + sizeof(*bs_hdr) + sizeof(crc);
    totlen = htons(totlen);

    memcpy(buf, &totlen, sizeof(totlen));
    totlen = sizeof(totlen);
    memcpy(&buf[totlen], bs_hdr, sizeof(*bs_hdr));
    totlen += sizeof(*bs_hdr);
    memcpy(&buf[totlen], data, len);
    totlen += len;
    memcpy(&buf[totlen], &crc, sizeof(crc));
    totlen += sizeof(crc);
    totlen = base64_encode(buf, totlen, encoded_buf, 1);
    console_write(encoded_buf, totlen);
    console_write("\n", 1);
}

/*
 * Task which waits reading console, expecting to get image over
 * serial port.
 */
static void
boot_serial(void *arg)
{
    int rc;
    int off;
    char *buf;
    int full_line;
    int max_input = (int)arg;

    rc = console_init(NULL);
    assert(rc == 0);
    console_echo(0);

    buf = os_malloc(max_input);
    assert(buf);

    off = 0;
    while (1) {
        rc = console_read(buf + off, max_input - off, &full_line);
        if (rc <= 0 && !full_line) {
            continue;
        }
        off += rc;
        if (!full_line) {
            continue;
        }
        if (buf[0] == SHELL_NLIP_PKT_START1 &&
          buf[1] == SHELL_NLIP_PKT_START2) {
            boot_serial_input(&buf[2], off - 2);
        }
        off = 0;
    }
}

int
boot_serial_task_init(struct os_task *task, uint8_t prio, os_stack_t *stack,
  uint16_t stack_size, int max_input)
{
    return os_task_init(task, "boot", boot_serial, (void *)max_input,
      prio, OS_WAIT_FOREVER, stack, stack_size);
}
