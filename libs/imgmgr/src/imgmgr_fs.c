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
#include <os/os.h>
#include <os/endian.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <newtmgr/newtmgr.h>
#include <bootutil/image.h>
#include <fs/fs.h>
#include <fs/fsutil.h>
#include <json/json.h>
#include <bsp/bsp.h>

#include "imgmgr/imgmgr.h"
#include "imgmgr_priv.h"

/* XXX share with bootutil */
#define BOOT_PATH		"/boot"
#define BOOT_PATH_MAIN          "/boot/main"
#define BOOT_PATH_TEST          "/boot/test"

#ifdef FS_PRESENT
static int
imgr_read_file(const char *path, struct image_version *ver)
{
    uint32_t bytes_read;
    int rc;

    rc = fsutil_read_file(path, 0, sizeof(*ver), ver, &bytes_read);
    if (rc != 0 || bytes_read != sizeof(*ver)) {
        return -1;
    }
    return 0;
}

static int
imgr_read_test(struct image_version *ver)
{
    return (imgr_read_file(BOOT_PATH_TEST, ver));
}

static int
imgr_read_main(struct image_version *ver)
{
    return (imgr_read_file(BOOT_PATH_MAIN, ver));
}

static int
imgr_write_file(const char *path, struct image_version *ver)
{
    return fsutil_write_file(path, ver, sizeof(*ver));
}

static void
imgr_ver_jsonstr(struct json_encoder *enc, char *key,
  struct image_version *ver)
{
    struct json_value jv;
    char ver_str[IMGMGR_NMGR_MAX_VER];
    int ver_len;

    ver_len = imgr_ver_str(ver, ver_str);
    JSON_VALUE_STRINGN(&jv, ver_str, ver_len);
    json_encode_object_entry(enc, key, &jv);
}

int
imgr_boot_read(struct nmgr_hdr *nmr, struct os_mbuf *req,
  uint16_t srcoff, struct nmgr_hdr *rsp_hdr, struct os_mbuf *rsp)
{
    int rc;
    struct json_encoder *enc;
    struct image_version ver;

    enc = &nmgr_task_jbuf.njb_enc;
    nmgr_jbuf_setobuf(&nmgr_task_jbuf, rsp_hdr, rsp);

    json_encode_object_start(enc);

    rc = imgr_read_test(&ver);
    if (!rc) {
        imgr_ver_jsonstr(enc, "test", &ver);
    }

    rc = imgr_read_main(&ver);
    if (!rc) {
        imgr_ver_jsonstr(enc, "main", &ver);
    }

    rc = imgr_read_ver(bsp_imgr_current_slot(), &ver);
    if (!rc) {
        imgr_ver_jsonstr(enc, "active", &ver);
    }

    json_encode_object_finish(enc);

    return 0;
}

int
imgr_boot_write(struct nmgr_hdr *nmr, struct os_mbuf *req,
  uint16_t srcoff, struct nmgr_hdr *rsp_hdr, struct os_mbuf *rsp)
{
    char test_ver_str[28];
    const struct json_attr_t boot_write_attr[2] = {
        [0] = {
            .attribute = "test",
            .type = t_string,
            .addr.string = test_ver_str,
            .len = sizeof(test_ver_str),
        },
        [1] = {
            .attribute = NULL
        }
    };
    int rc;
    struct image_version ver;

    nmgr_jbuf_setibuf(&nmgr_task_jbuf, req, srcoff + sizeof(*nmr),
      nmr->nh_len);

    rc = json_read_object(&nmgr_task_jbuf.njb_buf, boot_write_attr);
    if (rc) {
        return OS_EINVAL;
    }

    rc = imgr_ver_parse(boot_write_attr[0].addr.string, &ver);
    if (rc) {
        return OS_EINVAL;
    }

    fs_mkdir(BOOT_PATH);
    rc = imgr_write_file(BOOT_PATH_TEST, &ver);
    return rc;
}
#endif
