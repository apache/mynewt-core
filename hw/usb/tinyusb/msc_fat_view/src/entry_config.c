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

#include <stdint.h>
#include <stddef.h>
#include <ctype.h>
#include <stream/stream.h>
#include <msc_fat_view/msc_fat_view.h>
#include <modlog/modlog.h>
#include <hal/hal_flash.h>
#include <config/config.h>

struct config_export_stream {
    struct out_stream out_stream;
    /* Buffer to write to (can be NULL) */
    uint8_t *buffer;
    /* Stream start offset, writes to stream preceding this will be dropped */
    uint32_t buffer_start_offset;
    /* Stream end offset, writes to stream after this will be dropped */
    uint16_t buffer_end_offset;
    /* Current stream write offset */
    uint32_t write_offset;
};

static int
config_export_write(struct out_stream *ostream, const uint8_t *buf, uint32_t count)
{
    struct config_export_stream *str = (struct config_export_stream *)ostream;
    uint32_t upper_limit = str->write_offset + count;
    uint32_t lower_limit = str->write_offset;
    int cnt = count;

    if (lower_limit < str->buffer_end_offset && upper_limit > str->buffer_start_offset) {
        if (lower_limit < str->buffer_start_offset) {
            cnt -= str->buffer_start_offset - lower_limit;
            lower_limit = str->buffer_start_offset;
        }
        if (upper_limit > str->buffer_end_offset) {
            cnt -= upper_limit - str->buffer_end_offset;
        }
        memcpy(str->buffer + lower_limit - str->buffer_start_offset,
               buf + (lower_limit - str->write_offset), cnt);
    }
    str->write_offset += count;

    return count;
}

static int
config_export_flush(struct out_stream *ostream)
{
    return 0;
}

OSTREAM_DEF(config_export);

static struct config_export_stream export_stream = {
    .out_stream.vft = &config_export_vft,
};

static void
config_text_export(char *name, char *val)
{
    int name_len = strlen(name);
    int val_len = 0;

    if (val) {
        val_len = strlen(val);
    }
    ostream_write(&export_stream.out_stream, (const uint8_t *)name, name_len, false);
    ostream_write(&export_stream.out_stream, (const uint8_t *)" = ", 3, false);
    if (val) {
        ostream_write(&export_stream.out_stream, (const uint8_t *)val, val_len, false);
    }
    ostream_write(&export_stream.out_stream, (const uint8_t *)"\n", 1, false);
}

static const char *const CONFIG_BEGIN = "#### Config begin ####\n";
static const char *const CONFIG_END = "##### Config end #####\n";

void
config_txt_render_file(struct out_stream *stream)
{
    ostream_write_str(&export_stream.out_stream, CONFIG_BEGIN);
    conf_export(config_text_export, CONF_EXPORT_SHOW);
    ostream_write_str(&export_stream.out_stream, CONFIG_END);
}

static uint32_t
config_txt_size(const file_entry_t *file_entry)
{
    export_stream.buffer = NULL;
    export_stream.buffer_start_offset = 0;
    export_stream.buffer_end_offset = 0;
    export_stream.write_offset = 0;

    config_txt_render_file(&export_stream.out_stream);

    return export_stream.write_offset;
}

static void
config_txt_read(const struct file_entry *entry, uint32_t file_sector, uint8_t buffer[512])
{
    export_stream.buffer = buffer;
    export_stream.buffer_start_offset = 512 * file_sector;
    export_stream.buffer_end_offset = export_stream.buffer_start_offset + 512;
    export_stream.write_offset = 0;

    MSC_FAT_VIEW_LOG_DEBUG("Config.txt read sector %d\n", file_sector);
    config_txt_render_file(&export_stream.out_stream);
}

static uint8_t *line;
static size_t line_len;

enum config_write_state {
    WRITE_STATE_IDLE,
    WRITE_STATE_DROP,
    WRITE_STATE_CONFIG_LINES,
    WRITE_STATE_CONFIG_WRITTEN,

} config_write_state;

static struct os_callout reboot_callout;

static void
reboot_fun(struct os_event *ev)
{
    os_reboot(0);
}

static void
handle_line(uint8_t *bl, uint8_t *el)
{
    size_t len = el - bl;
    uint8_t *p;
    uint8_t *nb;
    uint8_t *ne;
    uint8_t *vb;

    if (config_write_state == WRITE_STATE_IDLE) {
        if (memcmp(CONFIG_BEGIN, bl, len) == 0) {
            config_write_state = WRITE_STATE_CONFIG_LINES;
            return;
        }
    } else if (config_write_state == WRITE_STATE_CONFIG_LINES) {
        if (memcmp(CONFIG_END, bl, len) == 0) {
            conf_commit(NULL);
            conf_save();
            config_write_state = WRITE_STATE_CONFIG_WRITTEN;
            os_callout_init(&reboot_callout, os_eventq_dflt_get(), reboot_fun, NULL);
            os_callout_reset(&reboot_callout, os_time_ms_to_ticks32(2000));
            return;
        }

        /* Trim line to first # */
        for (p = bl; p < el && *p != '#'; ++p) {
        }
        el = p;
        /* Trim trailing spaces */
        for (p = el - 1; p >= bl && isspace(*p); --p) {
        }
        el = p + 1;

        /* Trim leading spaces */
        for (p = bl; p < el && isspace(*p); ++p) {
        }
        if (p >= el) {
            return;
        }
        /* Variable name start found */
        nb = p;
        /* Find variable name end */
        for (; p < el && !isspace(*p) && *p != '='; ++p) {
        }
        ne = p;
        /* Skip spaces if any */
        for (; p < el && isspace(*p); ++p) {
        }
        /* If next character is not = nothing to set */
        if (p >= el || *p != '=') {
            return;
        }
        /* Trim spaces after = */
        for (++p; p < el && isspace(*p); ++p) {
        }
        if (p >= el) {
            return;
        }
        vb = p;
        /* Add string terminators */
        *ne = '\0';
        *el = '\0';
        conf_set_value((char *)nb, (char *)vb);
    }
}

static int
config_write_sector(uint32_t sector, uint8_t *buffer)
{
    uint8_t *p;
    uint8_t *e = buffer + 512;
    uint8_t *line_begin = buffer;
    uint8_t *bl;
    uint8_t *el;
    int c;

    for (p = buffer; p < e; ++p) {
        if (config_write_state == WRITE_STATE_CONFIG_WRITTEN) {
            config_write_state = WRITE_STATE_IDLE;
            return 512;
        } else if (config_write_state == WRITE_STATE_DROP) {
            return 0;
        } else {
            c = *p;
            if (c == '\r' || c == '\n') {
                if (line) {
                    line_len += (p - line_begin);
                    line = os_realloc(line, line_len + (p - line_begin));
                    bl = line;
                    el = bl + line_len;
                } else {
                    bl = line_begin;
                    el = p;
                    line_len = p - bl;
                }
                if (line_len) {
                    handle_line(bl, el);
                }
                os_free(line);
                line = NULL;
                line_begin = p + 1;
            } else if (iscntrl(c)) {
                if (config_write_state != WRITE_STATE_IDLE) {
                    config_write_state = WRITE_STATE_IDLE;
                    return 512;
                }
            }
        }
    }
    return config_write_state == WRITE_STATE_IDLE ? 0 : 512;
}


static void
config_txt_write(const struct file_entry *entry, uint32_t file_sector, uint8_t buffer[512])
{
    MSC_FAT_VIEW_LOG_DEBUG("Config.txt write sector %d\n", file_sector);

    config_write_sector(file_sector, buffer);
}

static int
config_write_sector_handler(struct msc_fat_view_write_handler *handler, uint32_t sector,
                            uint8_t *buffer)
{
    MSC_FAT_VIEW_LOG_DEBUG("config_write_sector_handler %d\n", (int)sector);

    return config_write_sector(sector, buffer);
}

static int
config_file_written(struct msc_fat_view_write_handler *handler, uint32_t size, uint32_t sector,
                    bool first_sector)
{
    MSC_FAT_VIEW_LOG_DEBUG("config_file_written %d %d\n", (unsigned)size, (unsigned )sector);

    return 0;
}

ROOT_DIR_ENTRY(config_txt, "CONFIG.TXT", FAT_FILE_ENTRY_ATTRIBUTE_FILE, config_txt_size,
               config_txt_read, config_txt_write, NULL, NULL);
MSC_FAT_VIEW_WRITE_HANDLER(config_handler, config_write_sector_handler, config_file_written);
