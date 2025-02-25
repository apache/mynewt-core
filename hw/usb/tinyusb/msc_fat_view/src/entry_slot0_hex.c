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

#include <stdio.h>
#include <sysflash/sysflash.h>
#include <msc_fat_view/msc_fat_view.h>
#include <bootutil/image.h>
#include <stream/stream.h>

/* One char for ':' */
#define RECORD_START_CHARS 1
/* Number of characters appended to record (2 checksum characters, CR LF) */
#define RECORD_END_CHARS 4
/* Number of data bytes for single data record */
#define RECORD_0_DATA_BYTES   16
#define RECORD_0_HEADER_BYTES 4
#define RECORD_CHARS(record_chars) (RECORD_START_CHARS + (record_chars) + RECORD_END_CHARS)

/* Number of record characters excluding ':' checksum (2 chars) and CR/LF */
#define RECORD_0_CHARS   RECORD_CHARS((RECORD_0_DATA_BYTES + RECORD_0_HEADER_BYTES) * 2)
#define RECORD_1_CHARS   RECORD_CHARS(8)
#define RECORD_4_CHARS   RECORD_CHARS(12)
/* Block is number of bytes that can be exported with just one record 4 */
#define BLOCK_SIZE 0x10000
/* Size in characters of memory block of n bytes */
#define BLOCK_CHARS(bytes) (RECORD_4_CHARS + (RECORD_0_CHARS * ((bytes) / RECORD_0_DATA_BYTES)))

static uint8_t
hex_digit(int v)
{
    v &= 0xF;

    return (v < 10) ? (v + '0') : (v + 'A' - 10);
}

/* Stream that receives records from and output whole lines to out_stream */
struct hex_file_line_format_stream {
    struct out_stream os;
    uint8_t pos;
    uint8_t sum;
    struct out_stream *out_stream;
};

/* Stream that generates content of Intel hex file */
struct hex_stream {
    struct in_stream is;

    const struct flash_area *fa;
    struct mem_out_stream memstr;
    size_t image_size;
};

/*
 * Function writes formated ascii text for number of bytes
 * It can be called first with record header:
 * (byte count, address, record type) (4 bytes) and then record data.
 * Or it can be called with whole record.
 * Function will add leading ':' compute and append sum and CR LF pair.
 */
static int
hex_file_line_format_stream_write(struct out_stream *ostream, const uint8_t *buf, uint32_t count)
{
    struct hex_file_line_format_stream *hx = (struct hex_file_line_format_stream *)ostream;
    struct out_stream *os = hx->out_stream;
    uint8_t ascii[2];
    uint8_t b;
    int n = (int)count;

    if (hx->pos == 0 && n > 0) {
        ostream_write_uint8(os, ':');
        hx->pos++;
        hx->sum = 0;
    }
    while (n > 0) {
        b = *buf++;
        hx->sum += b;
        ascii[0] = hex_digit(b >> 4);
        ascii[1] = hex_digit(b);
        ostream_write(os, ascii, 2, false);
        hx->pos += 2;
        n--;
    }
    return count;
}

static int
hex_file_line_format_stream_flush(struct out_stream *ostream)
{
    struct hex_file_line_format_stream *hx = (struct hex_file_line_format_stream *)ostream;
    struct out_stream *os = hx->out_stream;
    uint8_t ascii[4];
    uint8_t b;

    if (hx->pos >= 9) {
        b = (~hx->sum) + 1;
        ascii[0] = hex_digit(b >> 4);
        ascii[1] = hex_digit(b);
        if (RECORD_END_CHARS == 4) {
            ascii[2] = '\r';
            ascii[3] = '\n';
        } else {
            ascii[2] = '\n';
        }
        ostream_write(os, ascii, RECORD_END_CHARS, false);
        hx->pos = 0;
    }
    return ostream_flush(hx->out_stream);
}

OSTREAM_DEF(hex_file_line_format_stream);

static void
hex_stream_record_0(uint32_t addr, const uint8_t *data, struct out_stream *os)
{
    uint8_t buf[4] = { RECORD_0_DATA_BYTES, (uint8_t)(addr >> 8), (uint8_t)addr, 0x00 };

    ostream_write(os, buf, 4, false);
    ostream_write(os, data, RECORD_0_DATA_BYTES, true);
}

static void
hex_stream_record_1(struct out_stream *os)
{
    uint8_t buf[4] = { 0, 0, 0, 0x01 };

    ostream_write(os, buf, 4, true);
}

static void
hex_stream_record_4(uint32_t addr, struct out_stream *os)
{
    uint8_t data[6] = { 0x02, 0x00, 0x00, 0x04, (uint8_t)(addr >> 24), (uint8_t)(addr >> 16) };

    ostream_write(os, data, 6, true);
}

static int
hex_stream_available(struct in_stream *istream)
{
    return 0;
}

static int
hex_stream_read(struct in_stream *istream, uint8_t *buf, uint32_t count)
{
    struct hex_stream *his = (struct hex_stream *)istream;
    size_t mem_addr = his->fa->fa_off;
    size_t mem_offset = mem_addr & 0xFFFF;
    size_t mem_block_end = (mem_addr & 0xFFFF0000) + BLOCK_SIZE;
    size_t block_chars = BLOCK_CHARS(BLOCK_SIZE - mem_offset);
    int block_start = his->memstr.write_ptr;
    int block_end = block_start + block_chars;
    int skip_records;
    struct hex_file_line_format_stream hex_formater = {
        OSTREAM_INIT(hex_file_line_format_stream, os),
        .out_stream = (struct out_stream *)&his->memstr,
        .pos = 0,
        .sum = 0,
    };

    while (block_end <= 0) {
        block_start = block_end;
        block_end += BLOCK_CHARS(BLOCK_SIZE);
        his->memstr.write_ptr = block_start;
        mem_addr = mem_block_end;
        mem_block_end += BLOCK_SIZE;
    }
    /* Skip record 4 */
    if (his->memstr.write_ptr < -RECORD_4_CHARS) {
        his->memstr.write_ptr += RECORD_4_CHARS;
    }

    skip_records = -his->memstr.write_ptr / RECORD_0_CHARS;
    his->memstr.write_ptr += skip_records * RECORD_0_CHARS;
    mem_addr += skip_records * RECORD_0_DATA_BYTES;

    while (his->memstr.write_ptr < (int)count) {
        if (his->memstr.write_ptr < block_start + RECORD_4_CHARS) {
            hex_stream_record_4(mem_addr, &hex_formater.os);
        } else if (his->memstr.write_ptr >= block_end) {
            block_start = block_end;
            block_end += BLOCK_CHARS(BLOCK_SIZE);
            mem_addr = mem_block_end;
            mem_block_end += BLOCK_SIZE;
        } else if (mem_addr == his->fa->fa_off + his->image_size) {
            hex_stream_record_1(&hex_formater.os);
            break;
        } else {
            hex_stream_record_0(mem_addr, (const uint8_t *)mem_addr, &hex_formater.os);
            mem_addr += RECORD_0_DATA_BYTES;
        }
    }

    return 0;
}

ISTREAM_DEF(hex_stream);

struct hex_stream slot0_hx = {
    ISTREAM_INIT(hex_stream, is),
};

static void
slot0_hex_read(const struct file_entry *entry, uint32_t file_sector, uint8_t buffer[512])
{
    size_t file_offset = file_sector * 512;

    flash_area_open(FLASH_AREA_IMAGE_0, &slot0_hx.fa);

    mem_ostream_init(&slot0_hx.memstr, buffer, 512);
    slot0_hx.memstr.write_ptr = -file_offset;
    istream_read(&slot0_hx.is, buffer, 512);
}

static uint32_t
flash_size_to_hex_file_size(uint32_t start_addr, uint32_t size)
{
    uint32_t file_size = 0;
    uint32_t block_size = BLOCK_SIZE - (start_addr & 0x0000FFFF);

    size = (size + (RECORD_0_DATA_BYTES - 1)) & ~(RECORD_0_DATA_BYTES - 1);

    while (size > block_size) {
        file_size += BLOCK_CHARS(block_size);
        size -= block_size;
        block_size = BLOCK_SIZE;
    }
    file_size += BLOCK_CHARS(size);
    file_size += RECORD_1_CHARS;
    return file_size;
}

static uint32_t
slot0_hex_size(const file_entry_t *file)
{
    uint32_t size = 0;
    const struct flash_area *fa;
    struct image_tlv_info info;

    (void)file;

    if (0 == flash_area_open(FLASH_AREA_IMAGE_0, &fa)) {
        /* Read image size from flash image header */
        flash_area_read(fa, 12, &size, 4);
        size += sizeof(struct image_header);
        /* Add image TLV space if present */
        flash_area_read(fa, size, &info, 4);
        if (info.it_magic == 0x6907) {
            size += info.it_tlv_tot;
        }
        /* Round up so all records 0 are of the same size */
        size = (size + (RECORD_0_DATA_BYTES - 1)) & ~(RECORD_0_DATA_BYTES - 1);
        slot0_hx.image_size = size;

        /* Compute how many characters flash will produce in Intel Hex format */
        size = flash_size_to_hex_file_size(fa->fa_off, size);
        flash_area_close(fa);
    }

    return size;
}

ROOT_DIR_ENTRY(slot0_hex, "SLOT0.HEX", FAT_FILE_ENTRY_ATTRIBUTE_READ_ONLY, slot0_hex_size, slot0_hex_read, NULL, NULL, NULL);
