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

#include <string.h>
#include <assert.h>
#include "os/mynewt.h"
#include "modlog/modlog.h"
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "img_mgmt/img_mgmt.h"
#include "config/config.h"
#include "config/config_file.h"
#include "reboot/log_reboot.h"
#include "bsp/bsp.h"
#include "flash_map/flash_map.h"
#include "tinycbor/cbor.h"
#include "tinycbor/cbor_buf_writer.h"

uint16_t reboot_cnt;
static int8_t log_reboot_written;

static char *reboot_conf_get(int argc, char **argv, char *buf, int max_len);
static int reboot_conf_set(int argc, char **argv, char *val);
static int reboot_conf_export(void (*export_func)(char *name, char *val),
                              enum conf_export_tgt tgt);

struct conf_handler reboot_conf_handler = {
    .ch_name = "reboot",
    .ch_get = reboot_conf_get,
    .ch_set = reboot_conf_set,
    .ch_commit = NULL,
    .ch_export = reboot_conf_export
};

#if MYNEWT_VAL(REBOOT_LOG_FCB)
static struct fcb_log reboot_log_fcb;
static struct log reboot_log;
#if MYNEWT_VAL(LOG_FCB)
static struct flash_area reboot_sector;
#endif
#if MYNEWT_VAL(LOG_FCB2)
static struct flash_sector_range reboot_sector;
#endif
#endif


#if MYNEWT_VAL(REBOOT_LOG_CONSOLE)
static int
log_reboot_init_console(void)
{
    int rc;

    rc = modlog_register(LOG_MODULE_REBOOT, log_console_get(), LOG_SYSLEVEL,
                         NULL);
    if (rc != 0) {
        return rc;
    }

    return 0;

}
#endif

#if MYNEWT_VAL(REBOOT_LOG_FCB)
static int
log_reboot_init_fcb(void)
{
    const struct flash_area *ptr;
#if MYNEWT_VAL(LOG_FCB)
    struct fcb *fcbp;
#elif MYNEWT_VAL(LOG_FCB2)
    struct fcb2 *fcbp;
#endif
    int rc;

    if (flash_area_open(MYNEWT_VAL(REBOOT_LOG_FLASH_AREA), &ptr)) {
        return SYS_EUNKNOWN;
    }

    reboot_log_fcb.fl_entries = MYNEWT_VAL(REBOOT_LOG_ENTRY_COUNT);
    fcbp = &reboot_log_fcb.fl_fcb;
#if MYNEWT_VAL(LOG_FCB)
    reboot_sector = *ptr;
    fcbp->f_magic = 0x7EADBADF;
    fcbp->f_version = g_log_info.li_version;
    fcbp->f_sector_cnt = 1;
    fcbp->f_sectors = &reboot_sector;

    rc = fcb_init(fcbp);
    if (rc) {
        flash_area_erase(ptr, 0, ptr->fa_size);
        rc = fcb_init(fcbp);
        if (rc) {
            return rc;
        }
    }
#endif
#if MYNEWT_VAL(LOG_FCB2)
    fcbp->f_magic = 0x8EADBAE0;
    fcbp->f_version = g_log_info.li_version;
    fcbp->f_sector_cnt = 1;
    fcbp->f_range_cnt = 1;
    fcbp->f_ranges = &reboot_sector;
    reboot_sector.fsr_flash_area = *ptr;
    reboot_sector.fsr_sector_count = 1;
    reboot_sector.fsr_sector_size = ptr->fa_size;
    reboot_sector.fsr_align = flash_area_align(ptr);

    rc = fcb2_init(fcbp);
    if (rc) {
        flash_area_erase(ptr, 0, ptr->fa_size);
        rc = fcb2_init(fcbp);
        if (rc) {
            return rc;
        }
    }
#endif

    rc = log_register("reboot_log", &reboot_log, &log_fcb_handler,
                      &reboot_log_fcb, LOG_SYSLEVEL);
    if (rc != 0) {
        return rc;
    }

    rc = modlog_register(LOG_MODULE_REBOOT, &reboot_log, LOG_SYSLEVEL,
                         NULL);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
#endif

static int
reboot_cnt_inc(void)
{
    char str[12];
    int rc;

    reboot_cnt++;
    rc = conf_save_one("reboot/reboot_cnt",
                       conf_str_from_value(CONF_INT16, &reboot_cnt,
                                           str, sizeof(str)));
    return rc;
}

/**
 * Logs reboot with the specified reason
 * @param reason for reboot
 * @return 0 on success; non-zero on failure
 */
static int
log_reboot_write(const struct log_reboot_info *info)
{
    struct image_version ver;
    uint8_t hash[32];
    char buf[MYNEWT_VAL(REBOOT_LOG_BUF_SIZE)];
    uint8_t cbor_enc_buf[MYNEWT_VAL(REBOOT_LOG_BUF_SIZE)];
    int off;
    int rc;
    int i;
    struct cbor_buf_writer writer;
    struct CborEncoder enc;
    struct CborEncoder map;
    size_t cbor_buf_len;
    uint32_t flags;
    uint8_t state_flags;

#if MYNEWT_VAL(REBOOT_LOG_FCB)
    {
        const struct flash_area *ptr;
        if (flash_area_open(MYNEWT_VAL(REBOOT_LOG_FLASH_AREA), &ptr)) {
            return 0;
        }
    }
#endif

    rc = img_mgmt_read_info(boot_current_slot, &ver, hash, &flags);
    if (rc != 0) {
        return rc;
    }

    memset(cbor_enc_buf, 0, sizeof cbor_enc_buf);

    cbor_buf_writer_init(&writer, cbor_enc_buf, sizeof cbor_enc_buf);
    cbor_encoder_init(&enc, &writer.enc, 0);
    rc = cbor_encoder_create_map(&enc, &map, CborIndefiniteLength);
    if (rc != 0) {
        return rc;
    }

    cbor_encode_text_stringz(&map, "rsn");
    cbor_encode_text_stringz(&map, REBOOT_REASON_STR(info->reason));

    cbor_encode_text_stringz(&map, "cnt");
    cbor_encode_int(&map, reboot_cnt);

    cbor_encode_text_stringz(&map, "img");
    snprintf(buf, sizeof buf, "%u.%u.%u.%u",
                  ver.iv_major, ver.iv_minor, ver.iv_revision,
                  (unsigned int)ver.iv_build_num);
    cbor_encode_text_stringz(&map, buf);

    cbor_encode_text_stringz(&map, "hash");
    off = 0;
    for (i = 0; i < sizeof hash; i++) {
        off += snprintf(buf + off, sizeof buf - off, "%02x",
                        (unsigned int)hash[i]);
    }
    cbor_encode_text_stringz(&map, buf);

    if (info->file != NULL) {
        cbor_encode_text_stringz(&map, "die");
        off  = 0;

        /* If die filename is longer than 1/3 of total allocated
         * buffer, then trim the filename from left. */
        if (strlen(info->file) > ((sizeof buf) / 3)) {
            off = strlen(info->file) - ((sizeof buf) / 3);
        }
        snprintf(buf, sizeof buf, "%s:%d",
                &info->file[off], info->line);
        cbor_encode_text_stringz(&map, buf);
    }

    if (info->pc != 0) {
        cbor_encode_text_stringz(&map, "pc");
        cbor_encode_int(&map, info->pc);
    }

    state_flags = img_mgmt_state_flags(boot_current_slot);
    cbor_encode_text_stringz(&map, "flags");
    off = 0;
    buf[0] = '\0';

    if (state_flags & IMG_MGMT_STATE_F_ACTIVE) {
        off += snprintf(buf + off, sizeof buf - off, "%s ", "active");
    }
    if (!(flags & IMAGE_F_NON_BOOTABLE)) {
        off += snprintf(buf + off, sizeof buf - off, "%s ", "bootable");
    }
    if (state_flags & IMG_MGMT_STATE_F_CONFIRMED) {
        off += snprintf(buf + off, sizeof buf - off, "%s ", "confirmed");
    }
    if (state_flags & IMG_MGMT_STATE_F_PENDING) {
        off += snprintf(buf + off, sizeof buf - off, "%s ", "pending");
    }
    if (off > 1) {
        buf[off - 1] = '\0';
    }
    cbor_encode_text_stringz(&map, buf);

    /* Find length of the CBOR encoded log entry. */
    cbor_buf_len = cbor_buf_writer_buffer_size(&writer, cbor_enc_buf) + 1;

    rc = cbor_encoder_close_container(&enc, &map);
    if (rc != 0) {
        return SYS_ENOMEM;
    }

    /* Log a CBOR encoded reboot record. */
    modlog_append(LOG_MODULE_REBOOT, LOG_LEVEL_CRITICAL, LOG_ETYPE_CBOR,
                  cbor_enc_buf, cbor_buf_len);

    return 0;
}

int
log_reboot(const struct log_reboot_info *info)
{
    int rc;

    /* Don't log a second reboot entry. */
    if (log_reboot_written) {
        return 0;
    }

    rc = log_reboot_write(info);
    if (rc != 0) {
        return rc;
    }

    if (info->reason != HAL_RESET_REQUESTED &&
        info->reason != HAL_RESET_DFU) {
        /* Record that we have written a reboot entry for the current boot.
         * Upon rebooting, we won't write a second entry.
         */
        log_reboot_written = 1;
        conf_save_one("reboot/written", "1");
    }

    return 0;
}

/**
 * Increments the reboot counter and writes an entry to the reboot log, if
 * necessary.  This function should be called from main() after config
 * settings have been loaded via conf_load().
 *
 * @param reason                The cause of the reboot.
 */
void
reboot_start(enum hal_reset_reason reason)
{
    struct log_reboot_info info;

    /* If an entry wasn't written before the previous reboot, write one now. */
    if (!log_reboot_written) {
        reboot_cnt_inc();

        info = (struct log_reboot_info) {
            .reason = reason,
            .file = NULL,
            .line = 0,
            .pc = 0,
        };
        log_reboot_write(&info);
    }

    /* Record that we haven't written a reboot entry for the current boot. */
    log_reboot_written = 0;
    conf_save_one("reboot/written", "0");
}

static char *
reboot_conf_get(int argc, char **argv, char *buf, int max_len)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "reboot_cnt")) {
            return conf_str_from_value(CONF_INT16, &reboot_cnt, buf, max_len);
        } else if (!strcmp(argv[0], "written")) {
            return conf_str_from_value(CONF_BOOL, &log_reboot_written,
                                       buf, max_len);
        }
    }
    return NULL;
}

static int
reboot_conf_set(int argc, char **argv, char *val)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "reboot_cnt")) {
            return CONF_VALUE_SET(val, CONF_INT16, reboot_cnt);
        } else if (!strcmp(argv[0], "written")) {
            return CONF_VALUE_SET(val, CONF_INT16, log_reboot_written);
        }
    }

    return OS_ENOENT;
}

static int
reboot_conf_export(void (*func)(char *name, char *val),
                   enum conf_export_tgt tgt)
{
    char str[12];

    if (tgt == CONF_EXPORT_SHOW) {
        func("reboot/reboot_cnt",
             conf_str_from_value(CONF_INT16, &reboot_cnt, str, sizeof str));
        func("reboot/written",
             conf_str_from_value(CONF_BOOL, &log_reboot_written, str,
                                 sizeof str));
    }
    return 0;
}

void
log_reboot_pkg_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = conf_register(&reboot_conf_handler);
    SYSINIT_PANIC_ASSERT(rc == 0);

#if MYNEWT_VAL(REBOOT_LOG_FCB)
    rc = log_reboot_init_fcb();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
#if MYNEWT_VAL(REBOOT_LOG_CONSOLE)
    rc = log_reboot_init_console();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
}
