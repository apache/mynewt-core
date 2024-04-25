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
#include <stream/stream.h>
#include <msc_fat_view/msc_fat_view.h>
#include <modlog/modlog.h>
#include <hal/hal_flash.h>
#include <config/config.h>

struct config_export_stream {
    struct out_stream out_stream;
    uint8_t *buffer;
    uint32_t buffer_start_offset;
    uint16_t buffer_end_offset;
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

static uint32_t
config_txt_size(const file_entry_t *file_entry)
{
    export_stream.buffer = NULL;
    export_stream.buffer_start_offset = 0;
    export_stream.buffer_end_offset = 0;
    export_stream.write_offset = 0;

    conf_export(config_text_export, CONF_EXPORT_SHOW);

    return export_stream.write_offset;
}

static void
config_txt_read(const struct file_entry *entry, uint32_t file_sector, uint8_t buffer[512])
{
    export_stream.buffer = buffer;
    export_stream.buffer_start_offset = 512 * file_sector;
    export_stream.buffer_end_offset = export_stream.buffer_start_offset + 512;
    export_stream.write_offset = 0;

    MSC_FAT_VIEW_LOG_DEBUG("Config.txt read %d\n", file_sector);
    conf_export(config_text_export, CONF_EXPORT_SHOW);
}

ROOT_DIR_ENTRY(config_txt, "CONFIG.TXT", FAT_FILE_ENTRY_ATTRIBUTE_READ_ONLY, config_txt_size, config_txt_read, NULL, NULL);
