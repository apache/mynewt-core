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
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "sysinit/sysinit.h"
#include "hal/hal_bsp.h"
#include "os/os.h"
#include "config/config.h"
#include "base64/base64.h"
#include "mfg/mfg.h"

#include "id/id.h"

static char *id_conf_get(int argc, char **argv, char *val, int val_len_max);
static int id_conf_set(int argc, char **argv, char *val);
static int id_conf_export(void (*export_func)(char *name, char *val),
  enum conf_export_tgt tgt);

#define STAMP_STR(s) STAMP_STR1(s)
#define STAMP_STR1(str) #str

#ifdef BSP_NAME
static const char *bsp_str = STAMP_STR(BSP_NAME);
static const char *app_str = STAMP_STR(APP_NAME);
#else
static const char *bsp_str = "";
static const char *app_str = "";
#endif

char id_serial[ID_SERIAL_MAX_LEN];

/** Base64-encoded null-terminated manufacturing hash. */
char id_mfghash[BASE64_ENCODE_SIZE(MFG_HASH_SZ) + 1];

struct conf_handler id_conf = {
    .ch_name = "id",
    .ch_get = id_conf_get,
    .ch_set = id_conf_set,
    .ch_export = id_conf_export
};

static char *
id_conf_get(int argc, char **argv, char *val, int val_len_max)
{
    uint8_t src_buf[HAL_BSP_MAX_ID_LEN];
    int len;

    if (argc == 1) {
        if (!strcmp(argv[0], "hwid")) {
            len = hal_bsp_hw_id(src_buf, sizeof(src_buf));
            if (len > 0) {
                return conf_str_from_bytes(src_buf, len, val, val_len_max);
            }
        } else if (!strcmp(argv[0], "bsp")) {
            return (char *)bsp_str;
        } else if (!strcmp(argv[0], "app")) {
            return (char *)app_str;
        } else if (!strcmp(argv[0], "serial")) {
            return id_serial;
        } else if (!strcmp(argv[0], "mfghash")) {
            return id_mfghash;
        }
    }
    return NULL;
}

static int
id_conf_set(int argc, char **argv, char *val)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "serial")) {
            return CONF_VALUE_SET(val, CONF_STRING, id_serial);
        }
    }
    return OS_ENOENT;
}

static int
id_conf_export(void (*export_func)(char *name, char *val),
  enum conf_export_tgt tgt)
{
    uint8_t src_buf[HAL_BSP_MAX_ID_LEN];
    char str[sizeof(src_buf) * 2];
    int len;

    if (tgt == CONF_EXPORT_SHOW) {
        len = hal_bsp_hw_id(src_buf, sizeof(src_buf));
        if (len > 0) {
            conf_str_from_bytes(src_buf, len, str, sizeof(str));
        }
        export_func("id/hwid", str);
        export_func("id/bsp", (char *)bsp_str);
        export_func("id/app", (char *)app_str);
        export_func("id/mfghash", (char *)id_mfghash);
    }
    export_func("id/serial", id_serial);

    return 0;
}

static void
id_read_mfghash(void)
{
    uint8_t raw_hash[MFG_HASH_SZ];
    struct mfg_meta_tlv tlv;
    uint32_t off;
    int rc;

    memset(id_mfghash, 0, sizeof id_mfghash);

    /* Find hash TLV in the manufacturing meta region. */
    off = 0;
    rc = mfg_next_tlv_with_type(&tlv, &off, MFG_META_TLV_TYPE_HASH);
    if (rc != 0) {
        return;
    }

    /* Read the TLV contents. */
    rc = mfg_read_tlv_hash(&tlv, off, raw_hash);
    if (rc != 0) {
        return;
    }

    /* Store the SHA256 hash as a base64-encoded string. */
    base64_encode(raw_hash, sizeof raw_hash, id_mfghash, 1);
}

void
id_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = conf_register(&id_conf);
    SYSINIT_PANIC_ASSERT(rc == 0);

    /* Attempt to read the manufacturing image hash from the meta region. */
    id_read_mfghash();
}
