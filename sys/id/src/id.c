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

#include "os/mynewt.h"
#include "hal/hal_bsp.h"
#include "config/config.h"
#include "base64/base64.h"
#include "mfg/mfg.h"

#include "id/id.h"

#define ID_BASE64_MFG_HASH_SZ  (BASE64_ENCODE_SIZE(MFG_HASH_SZ))

static char *id_conf_get(int argc, char **argv, char *val, int val_len_max);
static int id_conf_set(int argc, char **argv, char *val);
static int id_conf_export(void (*export_func)(char *name, char *val),
  enum conf_export_tgt tgt);

#define STAMP_STR(s) STAMP_STR1(s)
#define STAMP_STR1(str) #str

#ifdef BSP_NAME
const char *id_bsp_str = STAMP_STR(BSP_NAME);
const char *id_app_str = STAMP_STR(APP_NAME);
#else
const char *id_bsp_str = "";
const char *id_app_str = "";
#endif

#if MYNEWT_VAL(ID_SERIAL_PRESENT)
char id_serial[ID_SERIAL_MAX_LEN];
#endif
#if MYNEWT_VAL(ID_MANUFACTURER_LOCAL)
char id_manufacturer[ID_MANUFACTURER_MAX_LEN];
#endif
#if MYNEWT_VAL(ID_MODEL_LOCAL)
char id_model[ID_MODEL_MAX_LEN];
#endif

/** Colon-delimited null-terminated list of base64-encoded mfgimage hashes. */
char id_mfghash[MYNEWT_VAL(MFG_MAX_MMRS) * (ID_BASE64_MFG_HASH_SZ + 1)];

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
            return (char *)id_bsp_str;
        } else if (!strcmp(argv[0], "app")) {
            return (char *)id_app_str;
#if MYNEWT_VAL(ID_SERIAL_PRESENT)
        } else if (!strcmp(argv[0], "serial")) {
            return (char *)id_serial;
#endif
#if MYNEWT_VAL(ID_MANUFACTURER_PRESENT)
        } else if (!strcmp(argv[0], "mfger")) {
            return (char *)id_manufacturer;
#endif
#if MYNEWT_VAL(ID_MODEL_PRESENT)
        } else if (!strcmp(argv[0], "model")) {
            return (char *)id_model;
#endif
#if MYNEWT_VAL(ID_TARGET_PRESENT)
        } else if (!strcmp(argv[0], "target")) {
            return MYNEWT_VAL(TARGET_NAME);
#endif
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
#if MYNEWT_VAL(ID_SERIAL_PRESENT)
        if (!strcmp(argv[0], "serial")) {
            return CONF_VALUE_SET(val, CONF_STRING, id_serial);
        }
#endif
#if MYNEWT_VAL(ID_MANUFACTURER_LOCAL)
        if (!strcmp(argv[0], "mfger")) {
            return CONF_VALUE_SET(val, CONF_STRING, id_manufacturer);
        }
#endif
#if MYNEWT_VAL(ID_MODEL_LOCAL)
        if (!strcmp(argv[0], "model")) {
            return CONF_VALUE_SET(val, CONF_STRING, id_model);
        }
#endif
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
        export_func("id/bsp", (char *)id_bsp_str);
        export_func("id/app", (char *)id_app_str);
        export_func("id/mfghash", (char *)id_mfghash);
#if MYNEWT_VAL(ID_TARGET_PRESENT)
        export_func("id/target", MYNEWT_VAL(TARGET_NAME));
#endif
    }
#if MYNEWT_VAL(ID_SERIAL_PRESENT)
    export_func("id/serial", id_serial);
#endif /* ID_SERIAL_PRESENT */
#if MYNEWT_VAL(ID_MANUFACTURER_PRESENT)
#if MYNEWT_VAL(ID_MANUFACTURER_LOCAL)
    export_func("id/mfger", id_manufacturer);
#else
    if (tgt == CONF_EXPORT_SHOW) {
        export_func("id/mfger", (char *)id_manufacturer);
    }
#endif /* ID_MANUFACTURER_LOCAL */
#endif /* ID_MANUFACTURER_PRESENT */
#if MYNEWT_VAL(ID_MODEL_PRESENT)
#if MYNEWT_VAL(ID_MODEL_LOCAL)
    export_func("id/model", id_model);
#else
    if (tgt == CONF_EXPORT_SHOW) {
        export_func("id/model", (char *)id_model);
    }
#endif /* ID_MODEL_LOCAL */
#endif /* ID_MODEL_PRESENT */
    return 0;
}

static void
id_read_mfghash(void)
{
    uint8_t raw_hash[MFG_HASH_SZ];
    struct mfg_reader reader;
    int str_off;
    int rc;

    memset(id_mfghash, 0, sizeof id_mfghash);

    mfg_open(&reader);

    str_off = 0;
    while (1) {
        rc = mfg_seek_next_with_type(&reader, MFG_META_TLV_TYPE_HASH);
        if (rc != 0) {
            return;
        }

        if (str_off + ID_BASE64_MFG_HASH_SZ + 1 > sizeof id_mfghash) {
            return;
        }

        /* Read the TLV contents. */
        rc = mfg_read_tlv_hash(&reader, raw_hash);
        if (rc != 0) {
            return;
        }

        /* Append a delimiter if this isn't the first hash. */
        if (str_off != 0) {
            id_mfghash[str_off] = ':';
            str_off++;
        }

        /* Append the SHA256 hash as a base64-encoded string. */
        base64_encode(raw_hash, sizeof raw_hash, &id_mfghash[str_off], 1);
        str_off += ID_BASE64_MFG_HASH_SZ;

        id_mfghash[str_off] = '\0';
    }
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
