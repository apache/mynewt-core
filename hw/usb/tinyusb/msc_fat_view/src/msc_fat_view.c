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
#include <os/endian.h>
#include <sysflash/sysflash.h>
#include <os/util.h>
#include <tusb.h>
#include <class/msc/msc.h>
#include <class/msc/msc_device.h>

#define FAT_TYPE_FAT12              12
#define FAT_TYPE_FAT16              16
#define FAT_TYPE_FAT32              32

#define FAT_TYPE                    MYNEWT_VAL(MSC_FAT_VIEW_FAT_TYPE)

#define SECTOR_COUNT                (MYNEWT_VAL(MSC_FAT_VIEW_DISK_SIZE) * 2)

#define SECTOR_SIZE                 512
#define SECTORS_PER_CLUSTER         (MYNEWT_VAL(MSC_FAT_VIEW_SECTORS_PER_CLUSTER))
#define CLUSTER_SIZE                ((SECTOR_SIZE) * (SECTORS_PER_CLUSTER))

#define FAT_ENTRY_SIZE              FAT_TYPE
#define FAT_ENTRY_COUNT             ((SECTOR_COUNT) / (SECTORS_PER_CLUSTER))
#define FAT_BYTES                   ((FAT_ENTRY_COUNT) * (FAT_ENTRY_SIZE) / 8)
#define FAT_SECTOR_COUNT            ((FAT_BYTES + SECTOR_SIZE - 1) / (SECTOR_SIZE))
#define SECTOR_BIT_COUNT            ((SECTOR_SIZE) * 8)

#if SECTOR_COUNT > 65535
#define SMALL_SECTOR_COUNT      0
#define LARGE_SECTOR_COUNT      SECTOR_COUNT
#else
#define SMALL_SECTOR_COUNT      SECTOR_COUNT
#define LARGE_SECTOR_COUNT      0
#endif

#if FAT_ENTRY_SIZE == FAT_TYPE_FAT12
#define FAT_ID  "FAT12   "
#elif FAT_ENTRY_SIZE == FAT_TYPE_FAT16
#define FAT_ID  "FAT16   "
#else
#define FAT_ID  "FAT32   "
#endif

struct TU_ATTR_PACKED boot_sector_start
{
    uint8_t jmp[3];
    char os_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_copies;
    uint16_t root_entries;
    uint16_t small_sectors;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t number_of_heads;
    uint32_t hidden_sectors;
    uint32_t large_sectors;
    uint8_t physical_disc_number;
    uint8_t current_head;
    uint8_t signature;
    uint8_t serial[4];
    char volume[11];
    char system_id[8];
};

struct TU_ATTR_PACKED boot_sector_start boot_sector_start = {
    .jmp = {0xEB, 0x3C, 0x90},
    .os_name = "MYNEWT  ",
    .bytes_per_sector = htole16(SECTOR_SIZE),
    .sectors_per_cluster = SECTORS_PER_CLUSTER,
    .reserved_sectors = htole16(1),
    .fat_copies = 1,
    .root_entries = htole16(16),
    .small_sectors = htole16(SMALL_SECTOR_COUNT),
    .media_descriptor = 0xF8,
    .sectors_per_fat = htole16(FAT_SECTOR_COUNT),
    .sectors_per_track = htole16(63),
    .number_of_heads = htole16(255),
    .hidden_sectors = htole16(0),
    .large_sectors = htole32(LARGE_SECTOR_COUNT),
    .physical_disc_number = 0x80,
    .current_head = 0,
    .signature = 0x29,
    .serial = "1234",
    .volume = MYNEWT_VAL(MSC_FAT_VIEW_VOLUME_NAME),
    .system_id = FAT_ID,
};

//struct dir_entry;

struct __attribute__((packed)) dir_entry {
    char short_name[11];
    const char *long_name;
    uint8_t attributes;
    uint32_t (*size)(const struct dir_entry *entry);
    void (*read)(uint32_t file_sector, uint8_t buffer[512]);
};

static uint32_t
return0(const struct dir_entry *entry)
{
    (void)entry;

    return 0;
}

static void
empty_read(uint32_t file_sector, uint8_t buffer[512])
{
    (void)file_sector;
    (void)buffer;
}

const struct dir_entry volume_label = {
    .short_name = MYNEWT_VAL(MSC_FAT_VIEW_VOLUME_NAME),
    .attributes = 0x08,
    .size = return0,
    .read = empty_read,
};

const char *const readme_text[] = {
    "This device runs Mynewt+TinyUSB on ",
    MYNEWT_VAL_BSP_NAME,
    "\n",
    NULL,
};

static uint32_t
readme_size(const struct dir_entry *dummy)
{
    uint32_t len = 0;
    const char * const *t = readme_text;

    while (*t) {
        len += strlen(*t);
        ++t;
    }
    return len;
}

static void
readme_read(uint32_t file_sector, uint8_t buffer[512])
{
    const char * const *t = readme_text;
    uint8_t *p = buffer;
    uint32_t len = 0;

    if (file_sector == 0) {
        while (*t) {
            len += strlen(*t);
            memcpy(p, *t, len);
            p += len;
            ++t;
        }
        memset(p, 0, buffer + 512 - p);
    } else {
        memset(buffer, 0, 512);
    }
}

const struct dir_entry readme = {
    .short_name = "README  TXT",
    .attributes = 0x01,
    .size = readme_size,
    .read = readme_read,
};

uint32_t
slot0_size(const struct dir_entry *dummy)
{
    uint32_t size = 0;
    const struct flash_area *fa;

    if (0 == flash_area_open(FLASH_AREA_IMAGE_0, &fa)) {
        size = fa->fa_size;
        flash_area_close(fa);
    }

    return size;
}

static void
slot0_read(uint32_t file_sector, uint8_t buffer[512])
{
    const struct flash_area *fa;
    uint32_t addr;

    if (0 == flash_area_open(FLASH_AREA_IMAGE_0, &fa)) {
        addr = file_sector * 512;
        flash_area_read(fa, addr, buffer, 512);
        flash_area_close(fa);
    }
}

const struct dir_entry slot0 = {
    .short_name = "FIRMWAREBIN",
    .attributes = 0x01,
    .size = slot0_size,
    .read = slot0_read,
};

static uint32_t
slot0_hex_size(const struct dir_entry *dummy)
{
    uint32_t size = 0;
    const struct flash_area *fa;

    if (0 == flash_area_open(FLASH_AREA_IMAGE_0, &fa)) {
        size = fa->fa_size * 4;
        flash_area_close(fa);
    }

    return size;
}

static uint8_t
hex_digit(int v)
{
    v &= 0xF;

    return (v < 10) ? (v + '0') : (v + 'A' - 10);
}

static void
slot0_hex_read(uint32_t file_sector, uint8_t buffer[512])
{
    const struct flash_area *fa;
    int i;
    int j;
    int k;
    uint32_t addr = (file_sector * 512) / 4;
    uint32_t addr_buf;

    if (0 == flash_area_open(FLASH_AREA_IMAGE_0, &fa)) {
        flash_area_read(fa, addr, buffer, 512 / 4);
        flash_area_close(fa);
        for (i = 512, j = 128; i > 0;) {
            buffer[--i] = '\n';
            buffer[--i] = '\r';
            for (k = 0; k < 16; ++k) {
                buffer[--i] = hex_digit(buffer[--j]);
                buffer[--i] = hex_digit(buffer[j] >> 4);
                buffer[--i] = ' ';
            }
            for (k = 0; k < 5; ++k) {
                buffer[--i] = ' ';
            }
            buffer[--i] = ':';
            addr_buf = addr + j;
            for (k = 0; k < 8; ++k) {
                buffer[--i] = hex_digit(addr_buf);
                addr_buf >>= 4;
            }
        }
    }
}

const struct dir_entry slot0_hex = {
    .short_name = "SLOT0   HEX",
    .attributes = 0x01,
    .size = slot0_hex_size,
    .read = slot0_hex_read,
};

const struct dir_entry system_volume_information = {
    .short_name = "SYSTEM~1   ",
    .long_name = "System Volume Information",
    .attributes = 0x26,
    .size = return0,
    .read = empty_read,
};

const struct dir_entry empty_file = {
    .short_name = "EMPTY      ",
    .long_name = "Empty",
    .attributes = 0x1,
    .size = return0,
    .read = empty_read,
};

static void
msc_read_boot_sector(uint8_t buffer[512])
{
    memcpy(buffer, &boot_sector_start, sizeof(boot_sector_start));
    memset(buffer + sizeof(boot_sector_start), 0, 512 - 2 - sizeof(boot_sector_start));
    buffer[510] = 0x55;
    buffer[511] = 0xAA;
}

const struct dir_entry *root_entries[] = {
    &volume_label,
    &system_volume_information,
    &readme,
    &empty_file,
    &slot0,
    &slot0_hex,
};

static void
entry_clusters(int entry, uint32_t *first_cluster, uint32_t *last_cluster)
{
    int i;
    uint32_t cluster;
    uint32_t size;
    uint32_t first = 0;
    uint32_t last = 0;

    if (entry >= 0 && entry < ARRAY_SIZE(root_entries)) {
        assert(root_entries[entry]->size);
        size = root_entries[entry]->size(root_entries[entry]);

        if (size) {
            cluster = 2;

            for (i = 0; i <= entry; ++i) {
                assert(root_entries[i]->size);
                size = root_entries[i]->size(root_entries[i]);
                if (size) {
                    first = cluster;
                    last = cluster + (size - 1) / CLUSTER_SIZE;
                    cluster = last + 1;
                } else {
                    first = 0;
                    last = 0;
                }
            }
        }
    }
    *first_cluster = first;
    *last_cluster = last;
}

static void
put_cluster_bits(uint8_t *buffer, uint32_t sector_start_bit, int cluster_bit_offset, uint32_t next_cluster)
{
    int ix;
    int drop_bits;

    if (FAT_ENTRY_SIZE == FAT_TYPE_FAT16) {
        ix = cluster_bit_offset / 8;
        buffer[(ix + 0) & (SECTOR_SIZE - 1)] = (uint8_t)(next_cluster);
        buffer[(ix + 1) & (SECTOR_SIZE - 1)] = (uint8_t)(next_cluster >> 8);
    } else if (FAT_ENTRY_SIZE == FAT_TYPE_FAT32) {
        ix = cluster_bit_offset / 8;
        buffer[(ix + 0) & (SECTOR_SIZE - 1)] = (uint8_t)(next_cluster);
        buffer[(ix + 1) & (SECTOR_SIZE - 1)] = (uint8_t)(next_cluster >> 8);
        buffer[(ix + 2) & (SECTOR_SIZE - 1)] = (uint8_t)(next_cluster >> 16);
        buffer[(ix + 3) & (SECTOR_SIZE - 1)] = (uint8_t)(next_cluster >> 24);
    } else {
        int bits = 12;
        if (sector_start_bit > cluster_bit_offset) {
            drop_bits = sector_start_bit - cluster_bit_offset;
            cluster_bit_offset += drop_bits;
            bits -= drop_bits;
            next_cluster >>= drop_bits;
        }
        while (bits && cluster_bit_offset < sector_start_bit + SECTOR_BIT_COUNT) {
            ix = (cluster_bit_offset - sector_start_bit) / 8;
            if ((cluster_bit_offset & 7) == 0) {
                if (bits > 4) {
                    buffer[ix] = (uint8_t)next_cluster;
                    next_cluster >>= 8;
                    cluster_bit_offset += 8;
                    bits -= 8;
                } else {
                    buffer[ix] &= 0xF0;
                    buffer[ix] |= (0x0F & next_cluster);
                    cluster_bit_offset -= 4;
                    next_cluster = 0;
                    bits = 0;
                }
            } else {
                buffer[ix] &= 0x0F;
                buffer[ix] |= (0x0F & next_cluster) << 4;
                bits -= 4;
                next_cluster >>= 4;
                cluster_bit_offset += 4;
            }
        }
    }
}

void
msc_fat_view_read_fat(uint16_t fat_sector, uint8_t buffer[512])
{
    int entry = 1;
    uint32_t cluster;
    uint32_t cluster_bits;
    uint32_t first_cluster = 0;
    uint32_t last_cluster = 0;
    uint32_t sector_start_bit;
    uint32_t sector_limit_bit;

    sector_start_bit = fat_sector * SECTOR_BIT_COUNT;
    sector_limit_bit = sector_start_bit + SECTOR_BIT_COUNT;
    if (fat_sector == 0) {
        cluster_bits = 2 * FAT_ENTRY_SIZE;
        cluster = 2;
        put_cluster_bits(buffer, sector_start_bit, 0, 0xFFFFFFF8);
        put_cluster_bits(buffer, sector_start_bit, FAT_ENTRY_SIZE, 0xFFFFFFFF);
    } else {
        cluster = sector_start_bit / FAT_ENTRY_SIZE;
        cluster_bits = cluster * FAT_ENTRY_SIZE;
    }

    for (; cluster_bits < sector_limit_bit; cluster_bits += FAT_ENTRY_SIZE, ++cluster) {
        for ( ; last_cluster < cluster_bits && entry < ARRAY_SIZE(root_entries); ++entry) {
            entry_clusters(entry, &first_cluster, &last_cluster);
            TU_LOG2("entry %d clusters %d..%d\n", entry, (int)first_cluster, (int)last_cluster);
            first_cluster *= FAT_ENTRY_SIZE;
            last_cluster *= FAT_ENTRY_SIZE;
        }
        if (first_cluster > cluster_bits + FAT_ENTRY_SIZE || cluster_bits > last_cluster) {
            put_cluster_bits(buffer, sector_start_bit, cluster_bits, 0);
        } else if (cluster_bits < last_cluster) {
            put_cluster_bits(buffer, sector_start_bit, cluster_bits, cluster + 1);
        } else if (cluster_bits == last_cluster) {
            put_cluster_bits(buffer, sector_start_bit, cluster_bits, 0xFFFFFFFF);
        }
    }
}

const uint8_t aaz[] = { 0x00, 0xC6, 0x52, 0x6D,
                        0x65, 0x43, 0x65, 0x43, 0x00, 0x00, 0x88, 0x6D, 0x65, 0x43 };

static uint8_t *
write_ucs_2(uint8_t *dst, const char **ascii, int len)
{
    for ( ; len; --len) {
        if (*ascii) {
            if (**ascii) {
                *dst++ = **ascii;
                *ascii += 1;
            } else {
                *dst++ = 0;
                *ascii = NULL;
            }
            *dst++ = 0;
        } else {
            *dst++ = 0xFF;
            *dst++ = 0xFF;
        }
    }
    return dst;
}

static uint8_t
short_name_checksum(const char *short_name)
{
    uint8_t checksum = 0;
    int i;

    for (i = 0; i < 11; ++i) {
        checksum = (((checksum & 1) ? 0x80 : 0) | (checksum >> 1)) + *short_name++;
    }

    return checksum;
}

static uint8_t *
write_long_name_entry(uint8_t *buffer, const char *long_name, const char *short_name)
{
    int len;
    int n;
    int i;
    uint8_t chksum;
    uint8_t *p;

    len = strlen(long_name);
    if (len) {
        chksum = short_name_checksum(short_name);
        n = (len + 12) / 13;
        buffer += 32 * n;
        for (i = 1; i <= n; ++i) {
            p = buffer - 32 * i;
            /* First part of long entry has the highest index and 6th bit set */
            *p++ = i + ((i == n) ? 0x40 : 0);
            p = write_ucs_2(p, &long_name, 5);
            *p++ = 0x0f;
            *p++ = 0x00; /* Unused */
            *p++ = chksum;
            p = write_ucs_2(p, &long_name, 6);
            *p++ = 0x00; /* Reserved */
            *p++ = 0x00; /* Reserved */
            (void)write_ucs_2(p, &long_name, 2);
        }
    }
    return buffer;
}

static void
msc_fat_view_read_root(uint16_t dir_sector, uint8_t buffer[512])
{
    const struct dir_entry *entry;
    int i;
    int cluster = 2;
    int cluster_count;
    uint32_t size;
    uint8_t *dst = buffer;

    TU_LOG1("msc_fat_view_read_root %d\n", dir_sector);
    for (i = 0; i < ARRAY_SIZE(root_entries); ++i) {
        entry = root_entries[i];
        if (entry->long_name) {
            dst = write_long_name_entry(dst, entry->long_name, entry->short_name);
        }
        memcpy(dst, root_entries[i]->short_name, 11);
        /* Clear out rest of the entry */
        memset(dst + 11, 0, 32 - 11);
        dst[0x0B] = root_entries[i]->attributes;
        memcpy(dst + 12, aaz, sizeof(aaz));
        if (i > 0) {
            assert(root_entries[i]->size);
            size = root_entries[i]->size(root_entries[i]);
            if (size) {
                cluster_count = 1 + (size - 1) / CLUSTER_SIZE;
                dst[0x1A] = (uint8_t)cluster;
                dst[0x1B] = (uint8_t)(cluster >> 8);
                dst[0x1C] = (uint8_t)(size >> 0);
                dst[0x1D] = (uint8_t)(size >> 8);
                dst[0x1E] = (uint8_t)(size >> 16);
                dst[0x1F] = (uint8_t)(size >> 24);
                cluster += cluster_count;
            }
        }
        dst += 32;
    }
    memset(dst, 0xE5, buffer + 512 - dst);
}

static void
msc_fat_view_read_sector(uint16_t sector, uint8_t buffer[512])
{
    int i;
    uint32_t cluster;
    uint32_t first_cluster;
    uint32_t last_cluster;

//    TU_LOG1("msc_read_sector %d\n", (int)sector);
    if (sector == 0) {
        msc_read_boot_sector(buffer);
    } else if (sector <= FAT_SECTOR_COUNT) {
        msc_fat_view_read_fat(sector - 1, buffer);
    } else if (sector == FAT_SECTOR_COUNT + 1) {
        msc_fat_view_read_root(0, buffer);
    } else {
        /* First cluster data is after boot, fat and root, it starts with 2 */
        cluster = sector - FAT_SECTOR_COUNT;
        for (i = 1; i < ARRAY_SIZE(root_entries); ++i) {
            entry_clusters(i, &first_cluster, &last_cluster);
            if (cluster >= first_cluster && cluster <= last_cluster) {
                root_entries[i]->read(cluster - first_cluster, buffer);
                break;
            }
        }
        if (i >= ARRAY_SIZE(root_entries)) {
            memset(buffer, 0, 512);
        }
    }
}

/*
 * Invoked when received SCSI_CMD_INQUIRY
 * Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
 */
void
tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
    (void)lun;

    const char vid[] = "Mynewt";
    const char pid[] = "Mass Storage";
    const char rev[] = "1.0";

    memcpy(vendor_id, vid, strlen(vid));
    memcpy(product_id, pid, strlen(pid));
    memcpy(product_rev, rev, strlen(rev));
}

/*
 * Invoked when received Test Unit Ready command.
 * return true allowing host to read/write this LUN e.g SD card inserted
 */
bool
tud_msc_test_unit_ready_cb(uint8_t lun)
{
    (void)lun;

    return true; // RAM disk is always ready
}

/*
 * Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
 * Application update block count and block size
 */
void
tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
    (void)lun;

    *block_count = SECTOR_COUNT;
    *block_size = SECTOR_SIZE;
}

/*
 * Invoked when received Start Stop Unit command
 * - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
 * - Start = 1 : active mode, if load_eject = 1 : load disk storage
 */
bool
tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
    (void)lun;
    (void)power_condition;

    if (load_eject) {
        if (start) {
            // load disk storage
        } else {
            // unload disk storage
        }
    }

    return true;
}

/*
 * Callback invoked when received READ10 command.
 * Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
 */
int32_t
tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
    (void)lun;
    (void)offset;

    msc_fat_view_read_sector(lba, buffer);

    return bufsize;
}

/*
 * Callback invoked when received WRITE10 command.
 * Process data in buffer to disk's storage and return number of written bytes
 */
int32_t
tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
    (void)lun;
    (void)lba;
    (void)offset;
    (void)buffer;
    TU_LOG1("SCSI WRITE10 %d, %d, %d\n", (int)lba, (int)offset, (int)bufsize);

    return bufsize;
}

/*
 * Callback invoked when received an SCSI command not in built-in list below
 * - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
 * - READ10 and WRITE10 has their own callbacks
 */
int32_t
tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize)
{
    // read10 & write10 has their own callback and MUST not be handled here

    void const *response = NULL;
    uint16_t resplen = 0;

    // most scsi handled is input
    bool in_xfer = true;

    TU_LOG1("SCSI cmd 0x%02X\n", scsi_cmd[0]);

    switch (scsi_cmd[0]) {
    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
        // Host is about to read/write etc ... better not to disconnect disk
        resplen = 0;
        break;

    default:
        // Set Sense = Invalid Command Operation
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

        // negative means error -> tinyusb could stall and/or response with failed status
        resplen = -1;
        break;
    }

    // return resplen must not larger than bufsize
    if (resplen > bufsize) resplen = bufsize;

    if (response && (resplen > 0)) {
        if (in_xfer) {
            memcpy(buffer, response, resplen);
        } else {
            // SCSI output
        }
    }

    return resplen;
}

void
usb_msc_mem_pkg_init(void)
{

}
