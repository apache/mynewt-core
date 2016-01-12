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

#include <newtmgr/newtmgr.h>
#include <bootutil/image.h>
#include <fs/fs.h>
#include <fs/fsutil.h>
#include <bsp/bsp.h>

#include "imgmgr_priv.h"

/* XXX share with bootutil */
#define BOOT_PATH		"/boot"
#define BOOT_PATH_MAIN          "/boot/main"
#define BOOT_PATH_TEST          "/boot/test"

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

int
imgr_boot_read(struct nmgr_hdr *nmr, struct os_mbuf *req,
  uint16_t srcoff, struct nmgr_hdr *rsp_hdr, struct os_mbuf *rsp)
{
    struct image_version ver;
    int rc;

    rc = imgr_read_test(&ver);
    if (rc) {
        memset(&ver, 0xff, sizeof(ver));
    }
    rc = imgr_nmgr_append_ver(rsp_hdr, rsp, &ver);
    if (rc) {
        goto err;
    }

    rc = imgr_read_main(&ver);
    if (rc) {
        memset(&ver, 0xff, sizeof(ver));
    }
    rc = imgr_nmgr_append_ver(rsp_hdr, rsp, &ver);
    if (rc) {
        goto err;
    }

    rc = imgr_read_ver(bsp_imgr_current_slot(), &ver);
    if (rc) {
        memset(&ver, 0xff, sizeof(ver));
    }
    rc = imgr_nmgr_append_ver(rsp_hdr, rsp, &ver);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

int
imgr_boot_write(struct nmgr_hdr *nmr, struct os_mbuf *req,
  uint16_t srcoff, struct nmgr_hdr *rsp_hdr, struct os_mbuf *rsp)
{
    struct image_version ver;
    int rc;
    int len;

    len = min(nmr->nh_len, sizeof(ver));
    rc = os_mbuf_copydata(req, srcoff + sizeof(*nmr), len, &ver);
    if (rc || len < sizeof(ver)) {
        return OS_EINVAL;
    }

    ver.iv_revision = ntohs(ver.iv_revision);
    ver.iv_build_num = ntohl(ver.iv_build_num);
    fs_mkdir(BOOT_PATH);
    rc = imgr_write_file(BOOT_PATH_TEST, &ver);
    return rc;
}
