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

#include <limits.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <newtmgr/newtmgr.h>
#include <bootutil/image.h>
#include <fs/fs.h>
#include <fs/fsutil.h>
#include <json/json.h>
#include <util/base64.h>
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
imgr_boot_read(struct nmgr_jbuf *njb)
{
    int rc;
    struct json_encoder *enc;
    struct image_version ver;

    enc = &njb->njb_enc;

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
imgr_boot_write(struct nmgr_jbuf *njb)
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

    rc = json_read_object(&njb->njb_buf, boot_write_attr);
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

int
imgr_file_upload(struct nmgr_jbuf *njb)
{
    char img_data[BASE64_ENCODE_SIZE(IMGMGR_NMGR_MAX_MSG)];
    char file_name[65];
    unsigned int off = UINT_MAX;
    unsigned int size = UINT_MAX;
    const struct json_attr_t off_attr[5] = {
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
        },
        [3] = {
            .attribute = "name",
            .type = t_string,
            .addr.string = file_name,
            .len = sizeof(file_name)
        }
    };
    struct json_encoder *enc;
    struct json_value jv;
    int rc;
    int len;

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
        imgr_state.upload.off = 0;
        imgr_state.upload.size = size;

        if (!strlen(file_name)) {
            return OS_EINVAL;
        }
        rc = fs_open(file_name, FS_ACCESS_WRITE | FS_ACCESS_TRUNCATE,
          &imgr_state.upload.file);
        if (rc) {
            return OS_EINVAL;
        }
    } else if (off != imgr_state.upload.off) {
        /*
         * Invalid offset. Drop the data, and respond with the offset we're
         * expecting data for.
         */
        rc = 0;
        goto out;
    }

    if (len && imgr_state.upload.file) {
        rc = fs_write(imgr_state.upload.file, img_data, len);
        imgr_state.upload.off += len;
        if (imgr_state.upload.size == imgr_state.upload.off) {
            /* Done */
            fs_close(imgr_state.upload.file);
            imgr_state.upload.file = NULL;
        }
    }
out:
    enc = &njb->njb_enc;

    json_encode_object_start(enc);

    JSON_VALUE_UINT(&jv, imgr_state.upload.off);
    json_encode_object_entry(enc, "off", &jv);
    json_encode_object_finish(enc);

    return 0;
}

#endif
