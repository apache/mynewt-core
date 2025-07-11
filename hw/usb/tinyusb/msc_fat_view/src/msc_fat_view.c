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
#include <ctype.h>
#include <os/endian.h>
#include <bsp/bsp.h>
#include <sysflash/sysflash.h>
#include <hal/hal_gpio.h>
#include <os/util.h>
#include <tusb.h>
#include <class/msc/msc.h>
#include <class/msc/msc_device.h>
#include <tinyusb/tinyusb.h>
#include <msc_fat_view/msc_fat_view.h>
#include <img_mgmt/img_mgmt.h>
#include <bootutil/image.h>
#include <modlog/modlog.h>
#include <hal/hal_flash.h>
#include <console/console.h>
#include <stream/stream.h>
#include "coredump_files.h"
#include <os/link_tables.h>

#if MYNEWT_VAL(BOOT_LOADER)
#define BOOT_LOADER     1
#define FLASH_AREA_IMAGE FLASH_AREA_IMAGE_0
#else
#define BOOT_LOADER     0
#ifdef FLASH_AREA_IMAGE_1
#define FLASH_AREA_IMAGE FLASH_AREA_IMAGE_1
#endif
#endif

#define FAT_TYPE_FAT12              12
#define FAT_TYPE_FAT16              16
#define FAT_TYPE_FAT32              32

#define FAT_COUNT                   1
#define SECTOR_COUNT                (MYNEWT_VAL(MSC_FAT_VIEW_DISK_SIZE) * 2)

#define SECTORS_PER_CLUSTER         (MYNEWT_VAL(MSC_FAT_VIEW_SECTORS_PER_CLUSTER))
#define CLUSTER_SIZE                ((SECTOR_SIZE) * (SECTORS_PER_CLUSTER))

#define CLUSTER_COUNT               ((SECTOR_COUNT + ((SECTORS_PER_CLUSTER) - 1)) / (SECTORS_PER_CLUSTER))

#if CLUSTER_COUNT < 4085
#define FAT_TYPE                    FAT_TYPE_FAT12
/* TODO: Add FAT12 support */
#error FAT12 not supported yet
#elif CLUSTER_COUNT < 65525
#define FAT_TYPE                    FAT_TYPE_FAT16
#else
#error FAT32 not supported yet
#define FAT_TYPE                    FAT_TYPE_FAT32
#endif

#define FAT_ENTRY_SIZE              FAT_TYPE
#define FAT_ENTRY_COUNT             ((SECTOR_COUNT) / (SECTORS_PER_CLUSTER))
#define FAT_BYTES                   ((FAT_ENTRY_COUNT) * (FAT_ENTRY_SIZE) / 8)
#define FAT_SECTOR_COUNT            ((FAT_BYTES + SECTOR_SIZE - 1) / (SECTOR_SIZE))
#define SECTOR_BIT_COUNT            ((SECTOR_SIZE) * 8)

#define DIR_ENTRY_SIZE              32
#define ROOT_SECTOR_COUNT           MYNEWT_VAL(MSC_FAT_VIEW_ROOT_DIR_SECTORS)

#define FAT_FIRST_SECTOR            1
#define FAT_ROOT_DIR_FIRST_SECTOR   ((FAT_FIRST_SECTOR) + (FAT_SECTOR_COUNT) * (FAT_COUNT))
#define FAT_CLUSTER2_FIRST_SECTOR   ((FAT_ROOT_DIR_FIRST_SECTOR) + (ROOT_SECTOR_COUNT))

#if SECTOR_COUNT > 65535
#define SMALL_SECTOR_COUNT      0
#define LARGE_SECTOR_COUNT      SECTOR_COUNT
#else
#define SMALL_SECTOR_COUNT      SECTOR_COUNT
#define LARGE_SECTOR_COUNT      0
#endif

#if FAT_ENTRY_SIZE == FAT_TYPE_FAT12
#define FAT_ID  "FAT12   "
typedef uint16_t cluster_t;
#elif FAT_ENTRY_SIZE == FAT_TYPE_FAT16
#define FAT_ID  "FAT16   "
typedef uint16_t cluster_t;
#else
#define FAT_ID  "FAT32   "
typedef uint32_t cluster_t;
#endif

#define FAT_CHAIN_END   ((cluster_t)0xFFFFFFFF)

#if MYNEWT_VAL(MSC_FAT_VIEW_HUGE_FILE)
#if MYNEWT_VAL(MSC_FAT_VIEW_HUGE_FILE_SIZE) > 0
#define HUGE_FILE_SIZE          MYNEWT_VAL(MSC_FAT_VIEW_HUGE_FILE_SIZE)
#if HUGE_FILE_SIZE > (MYNEWT_VAL(MSC_FAT_VIEW_DISK_SIZE) * 1024) + 2000000
#error HUGE_FILE_SIZE is to big for specified disk size
#endif
#elif (MYNEWT_VAL(MSC_FAT_VIEW_DISK_SIZE) * 1024) < 2000000
#error No space for huge file, increase MSC_FAT_VIEW_DISK_SIZE in syscfg
#else
#define HUGE_FILE_SIZE          ((MYNEWT_VAL(MSC_FAT_VIEW_DISK_SIZE) * 1024) - 2000000)
#endif
#else
#define HUGE_FILE_SIZE          0
#endif

struct TU_ATTR_PACKED boot_sector_start {
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
    .root_entries = htole16(ROOT_SECTOR_COUNT * SECTOR_SIZE / DIR_ENTRY_SIZE),
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

typedef union {
    uint8_t bytes[32];
    struct TU_ATTR_PACKED {
        char name[8];
        char ext[3];
        uint8_t attr;
        uint8_t reserved1;
        uint8_t creat_time[5];
        uint8_t access_time[2];
        uint16_t cluster_hi;
        uint8_t write_time[4];
        uint16_t cluster_lo;
        uint32_t size;
    };
    struct TU_ATTR_PACKED {
        uint8_t sequence;
        uint16_t name1[5];
        uint8_t attr1;
        uint8_t reserved2;
        uint8_t checksum;
        uint16_t name2[6];
        uint16_t reserved3;
        uint16_t name3[2];
    };
} fat_dir_entry_t;

typedef struct fat_chain {
    cluster_t first;
    cluster_t count;
    cluster_t next_chain;
} fat_chain_t;

typedef struct dir_entry {
    const file_entry_t *file;
    uint8_t dir_slots;
    uint8_t deleted;
    cluster_t first_cluster;
} dir_entry_t;

struct fat_chain fat_chains[32];
/* Number of chains in fat_chains array */
uint8_t fat_chain_count;
/* Number of free clusters */
cluster_t free_clusters;

static dir_entry_t root_dir[16];
static uint8_t root_dir_entry_count;

static scsi_cmd_type_t last_scsi_command;

typedef enum {
    MEDIUM_NOT_PRESENT,
    REPORT_MEDIUM_CHANGE,
    MEDIUM_RELOAD,
    MEDIUM_PRESENT,
} medium_state_t;

static medium_state_t medium_state;

static uint32_t
return0(const file_entry_t *file)
{
    (void)file;

    return 0;
}

static void
empty_read(const struct file_entry *entry, uint32_t file_sector, uint8_t buffer[512])
{
    (void)entry;
    (void)file_sector;
    (void)buffer;
}

static const file_entry_t volume_label = {
    .name = MYNEWT_VAL(MSC_FAT_VIEW_VOLUME_NAME),
    .attributes = FAT_FILE_ENTRY_ATTRIBUTE_LABEL,
    .size = return0,
    .read_sector = empty_read,
};

static const file_entry_t system_volume_information = {
    .name = "System Volume Information",
    .attributes = FAT_FILE_ENTRY_ATTRIBUTE_ARCHIVE |
                  FAT_FILE_ENTRY_ATTRIBUTE_SYSTEM |
                  FAT_FILE_ENTRY_ATTRIBUTE_HIDDEN,
    .size = return0,
    .read_sector = empty_read,
};

static const file_entry_t drop_image_here = {
    .name = "Drop image here",
    .attributes = FAT_FILE_ENTRY_ATTRIBUTE_READ_ONLY,
    .size = return0,
    .read_sector = empty_read,
};

static int write_status;
static const char *write_result_text[] = {
    "File that was written was not a valid image.",
    "Current image not confirmed, new image rejected.",
    "File write error.",
};

static uint32_t
flash_result_create_content(struct MemFile *file)
{
    int ix = abs(write_status) - 1;
    if (ix > 2) {
        ix = 2;
    }
    fwrite(write_result_text[ix], 1, strlen(write_result_text[ix]), &file->file);

    return file->bytes_written;
}

static uint32_t
flash_result_size(const file_entry_t *file_entry)
{
    struct MemFile sector_file;

    (void)file_entry;
    fmemopen_w(&sector_file, (char *)NULL, 0);

    flash_result_create_content(&sector_file);

    return sector_file.bytes_written;
}

static void
flash_result_read(const struct file_entry *entry, uint32_t file_sector, uint8_t buffer[512])
{
    struct MemFile sector_file;
    int written = 0;
    (void)entry;

    if (file_sector == 0) {
        fmemopen_w(&sector_file, (char *)buffer, 512);
        flash_result_create_content(&sector_file);
        written = sector_file.bytes_written;
    }

    memset(buffer + written, 0, 512 - written);
}

const file_entry_t flash_result = {
    .name = "Write error.txt",
    .attributes = FAT_FILE_ENTRY_ATTRIBUTE_READ_ONLY,
    .size = flash_result_size,
    .read_sector = flash_result_read,
};

/*
 * Return number of directory entries required for given file_name.
 *
 * File name consisting only of upper case letters and numbers up
 * to 8 characters of name plus up to 3 characters of extension
 * take one slot. If there is multiply dots, lower case letters,
 * file name is longer then 8 or extension is longer then 3,
 * additional slots are required.
 */
static uint8_t
fat_dir_entry_slots(const char *file_name)
{
    uint8_t slots = 1;
    size_t len = strlen(file_name);
    int dot_pos = -1;
    int i = 0;
    int c;

    if (len <= 12) {
        for (i = 0; i < len; ++i) {
            c = (uint8_t)file_name[i];
            if (c == '.') {
                if (dot_pos < 0 && i > 0 && i < 9) {
                    dot_pos = i;
                    continue;
                }
            }
            if ((isupper(c) || isdigit(c)) && (i < 8 || (dot_pos > 0 && i <= dot_pos + 3))) {
                continue;
            }
            break;
        }
    }
    if (i != len) {
        slots += (len + 12) / 13;
    }
    return slots;
}

static uint32_t
cluster_to_sector(cluster_t cluster)
{
    return ((cluster - 2) * SECTORS_PER_CLUSTER) + FAT_CLUSTER2_FIRST_SECTOR;
}

static void
sector_to_cluster(uint32_t sector, cluster_t *cluster, uint32_t *sector_in_cluster)
{
    sector -= FAT_CLUSTER2_FIRST_SECTOR;
    *cluster = sector / SECTORS_PER_CLUSTER;
    *sector_in_cluster = sector - (*cluster * SECTORS_PER_CLUSTER);
    *cluster += 2;
}

static cluster_t
cluster_count_from_bytes(uint32_t file_size)
{
    return 1 + (file_size - 1) / CLUSTER_SIZE;
}

static fat_chain_t *
fat_chain_find(cluster_t cluster)
{
    fat_chain_t *limit = fat_chains + fat_chain_count;
    fat_chain_t *chain;

    MSC_FAT_VIEW_LOG_DEBUG("fat_chain_find(%d)\n", cluster);
    for (chain = fat_chains; chain < limit; ++chain) {
        MSC_FAT_VIEW_LOG_DEBUG("chain %d %d-%d\n", chain - fat_chains, chain->first, chain->first + chain->count - 1);
        if (cluster >= chain->first + chain->count) {
            continue;
        }
        return (cluster < (chain->first + chain->count)) ? chain : NULL;
    }
    return NULL;
}

static void
fat_chain_append(cluster_t first_cluster, fat_chain_t *tail)
{
    fat_chain_t *limit = fat_chains + fat_chain_count;
    fat_chain_t *chain = fat_chains;

    while (chain < limit) {
        if (chain->first == first_cluster) {
            /* Found last chain */
            if (chain->next_chain == 0) {
                chain->next_chain = tail->first;
                break;
            } else {
                if (first_cluster > chain->next_chain) {
                    first_cluster = chain->next_chain;
                    chain = fat_chains;
                } else {
                    first_cluster = chain->next_chain;
                    chain++;
                }
            }
        }
    }
}

static void
fat_chain_insert(fat_chain_t *chain)
{
    fat_chain_t *limit = fat_chains + fat_chain_count + 1;

    fat_chain_count++;
    for (; limit > chain; --limit) {
        limit[0] = limit[-1];
    }
}

static void
fat_chain_remove(fat_chain_t *chain)
{
    fat_chain_t *limit = fat_chains + fat_chain_count;

    free_clusters += chain->count;
    for (limit--; chain < limit; chain++) {
        chain[0] = chain[1];
    }
    fat_chain_count--;
}

static void
fat_chain_insert_short(fat_chain_t *chain, cluster_t cluster, cluster_t next_cluster)
{
    if (next_cluster == 0) {
        return;
    }

    fat_chain_insert(chain);
    chain->first = cluster;
    if (cluster + 1 == next_cluster) {
        chain->count = 2;
        chain->next_chain = 0;
    } else {
        chain->count = 1;
        chain->next_chain = (next_cluster != FAT_CHAIN_END) ? next_cluster : 0;
    }
    free_clusters -= chain->count;
}

static void
fat_modify_next_cluster(cluster_t cluster, cluster_t next_cluster, fat_chain_t **cache)
{
    fat_chain_t *limit = fat_chains + fat_chain_count;
    fat_chain_t *chain = *cache;

    if (chain == NULL) {
        chain = fat_chains;
    }
    /* Skip chains that end before */
    while (chain < limit && chain->first + chain->count <= cluster) {
        chain++;
    }
    if (chain == limit) {
        /* Chain after every other chain, just append short one */
        fat_chain_insert_short(chain, cluster, next_cluster);
    } else if (cluster == chain->first - 1 && next_cluster == chain->first) {
        /* Chain extends next chain by one cluster from the beginning */
        chain->first = cluster;
        chain->count++;
    } else if (cluster < chain->first) {
        /* Chain in the middle of free space */
        fat_chain_insert_short(chain, cluster, next_cluster);
    } else if (cluster + 1 == next_cluster) {
        if (chain->first + chain->count == next_cluster) {
            /* chain just get extended by one */
            chain->count++;
            chain->next_chain = 0;
            if (chain + 1 < limit && chain[1].first == next_cluster) {
                chain[1].first++;
                chain[1].count--;
                if (chain[1].count == 0) {
                    fat_chain_remove(chain + 1);
                }
            }
        } else {
            /* no change in chain */
        }
    } else if (chain->first == cluster && next_cluster == 0) {
        /* First cluster remove from the chain, this is normal case when file
         * is deleted */
        chain->first++;
        chain->count--;
        if (chain->count == 0) {
            fat_chain_remove(chain);
        }
    } else if (chain->first + chain->count - 1 == cluster) {
        if (next_cluster != FAT_CHAIN_END && next_cluster != 0) {
            /* This normal case when file is extended in separate chain */
            chain->next_chain = next_cluster;
        } else {
            chain->next_chain = 0;
        }
    } else if (chain->first + chain->count - 1 > cluster) {
        /* Chain is no longer in once piece */
        fat_chain_insert(chain);
        chain->count = cluster - chain->first + 1;
        chain[1].first = cluster + 1;
        chain[1].count -= chain->count;
        if (next_cluster != 0 && next_cluster != FAT_CHAIN_END) {
            chain->next_chain = next_cluster;
        } else {
            chain->next_chain = 0;
        }
    }
}

static cluster_t
alloc_cluster_chain(cluster_t first_cluster, cluster_t cluster_count)
{
    fat_chain_t *limit = fat_chains + fat_chain_count;
    fat_chain_t *chain = fat_chains;
    fat_chain_t *prev = NULL;
    fat_chain_t *allocated = NULL;
    cluster_t c = 2;
    cluster_t free_space;

    if (free_clusters < cluster_count) {
        return 0;
    }
    free_clusters -= cluster_count;

    for (; chain < limit; ++chain) {
        if (c < chain->first) {
            if (prev) {
                prev->next_chain = c;
            }
            free_space = chain->first - c;
            /* Found first free space, move chains up */
            fat_chain_insert(chain);
            limit++;
            chain->first = c;
            chain->count = min(cluster_count, free_space);
            cluster_count -= chain->count;
            if (allocated == NULL) {
                allocated = chain;
            }
            if (cluster_count == 0) {
                break;
            }
            prev = chain;
        }
        c = chain->first + chain->count;
    }
    if (cluster_count) {
        if (prev) {
            prev->next_chain = c;
        }
        chain->first = c;
        chain->count = cluster_count;
        fat_chain_count++;
    }

    if (first_cluster) {
        fat_chain_append(first_cluster, chain);
    }

    return chain->first;
}

void
free_cluster_chain(cluster_t cluster)
{
    fat_chain_t *limit = fat_chains + fat_chain_count;
    fat_chain_t *chain = fat_chains;

    while (cluster != 0 && chain < limit) {
        if (cluster == chain->first) {
            cluster = chain->next_chain;
            fat_chain_remove(chain);
            limit--;
            /* Continuation is behind, start from the beginning */
            if (chain < limit && chain->first > cluster) {
                chain = fat_chains;
            }
        } else {
            chain++;
        }
    }
}

void
msc_fat_view_add_dir_entry(const file_entry_t *file)
{
    int entry_ix = root_dir_entry_count++;
    medium_state_t state = medium_state;

    if (state != MEDIUM_NOT_PRESENT) {
        medium_state = MEDIUM_NOT_PRESENT;
    }
    root_dir[entry_ix].file = file;
    root_dir[entry_ix].dir_slots = fat_dir_entry_slots(file->name);
    MSC_FAT_VIEW_LOG_DEBUG("Added root entry %s\n", file->name);
    if (state != MEDIUM_NOT_PRESENT) {
        medium_state = MEDIUM_RELOAD;
    }
}

void
msc_fat_view_update_dir_entry(int entry_ix)
{
    uint32_t file_size;
    const file_entry_t *file = root_dir[entry_ix].file;

    if (!root_dir[entry_ix].deleted) {
        root_dir[entry_ix].first_cluster = 0;
        file_size = file->size(file);
        if (file_size > 0) {
            root_dir[entry_ix].first_cluster = alloc_cluster_chain(0, cluster_count_from_bytes(file_size));
        }
        MSC_FAT_VIEW_LOG_DEBUG("Root file %s size %d, cluster %d (%d)\n", file->name, file_size,
                               root_dir[entry_ix].first_cluster,
                               cluster_count_from_bytes(file_size));
    }
}

static dir_entry_t *
msc_fat_view_find_dir_entry(const char *name)
{
    dir_entry_t *entry = root_dir;
    dir_entry_t *limit = root_dir + root_dir_entry_count;

    for (; entry < limit; ++entry) {
        if (strcmp(entry->file->name, name) == 0) {
            return entry;
        }
    }
    return NULL;
}

static dir_entry_t *
msc_fat_view_dir_entry_from_cluster(cluster_t cluster, cluster_t *cluster_in_chain)
{
    int i;
    fat_chain_t *chain = fat_chains;
    fat_chain_t *limit = fat_chains + fat_chain_count;

    while (chain < limit && cluster >= chain->first + chain->count) {
        ++chain;
    }
    if (chain >= limit || chain->first > cluster) {
        *cluster_in_chain = 0;
        return NULL;
    }
    /* TODO: find previous chain */
    *cluster_in_chain = cluster - chain->first;
    for (i = 0; i < root_dir_entry_count; ++i) {
        if (root_dir[i].first_cluster == chain->first) {
            return &root_dir[i];
        }
    }
    return NULL;
}

static void
msc_fat_view_put_cluster_bits(uint8_t *buffer, uint32_t sector_start_bit, int cluster_bit_offset,
                              uint32_t next_cluster)
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

static cluster_t
msc_fat_view_get_cluster_bits(const uint8_t *buffer, uint32_t sector_start_bit, int cluster_bit_offset)
{
    int ix;
    cluster_t next_cluster;

    if (FAT_ENTRY_SIZE == FAT_TYPE_FAT16) {
        ix = cluster_bit_offset / 8;
        next_cluster = buffer[(ix + 0) & (SECTOR_SIZE - 1)] |
                       buffer[(ix + 1) & (SECTOR_SIZE - 1)] << 8;
    } else if (FAT_ENTRY_SIZE == FAT_TYPE_FAT32) {
        ix = cluster_bit_offset / 8;
        next_cluster = buffer[(ix + 0) & (SECTOR_SIZE - 1)] |
                       buffer[(ix + 1) & (SECTOR_SIZE - 1)] << 8 |
                       buffer[(ix + 1) & (SECTOR_SIZE - 1)] << 16 |
                       buffer[(ix + 1) & (SECTOR_SIZE - 1)] << 24;
    } else {
        /* TODO: FAT12 support */
    }
    return next_cluster;
}

static cluster_t
msc_fat_view_fat_next_cluster(cluster_t cluster, fat_chain_t **cache)
{
    fat_chain_t *chain;
    fat_chain_t *limit = fat_chains + fat_chain_count;

    assert(cache != NULL);

    chain = *cache;
    if (chain == NULL) {
        chain = fat_chains;
    }
    /* Skip chains that end before cluster */
    while (cluster >= chain->first + chain->count) {
        ++chain;
    }
    /*
     * Keep it as cache, it may be a chain that cluster belongs to or next chain
     * if the cluster is free.
     */
    *cache = chain;
    /* Chain is of limit cluster is free
     * The cluster lays before chain, cluster is free
     */
    if (chain >= limit || cluster < chain->first) {
        return 0;
    } else if (cluster < chain->first + chain->count - 1) {
        /* Cluster is in the middle of chain, next cluster is next in sequence */
        return cluster + 1;
    } else if (chain->next_chain) {
        /* Chain is not continuous, next cluster is directly set in chain */
        return chain->next_chain;
    } else {
        /* The cluster is last in chain and there is no more chains for this file */
        return FAT_CHAIN_END;
    }
}

static void
msc_fat_view_write_ucs_2(struct out_stream *ostr, const char *ascii, int field_len)
{
    for (; ascii && field_len; --field_len) {
        ostream_write_uint8(ostr, *ascii);
        ostream_write_uint8(ostr, 0);
        if (*ascii == '\0') {
            ascii = NULL;
        } else {
            ++ascii;
        }
    }
    for (; field_len; --field_len) {
        ostream_write_uint16(ostr, 0xFFFF);
    }
}

static void
msc_fat_view_read_ucs_2(char *utf, const uint8_t *ucs, int len)
{
    for ( ; len; --len) {
        if (ucs[0] == 0xFF && ucs[1] == 0xFF) {
            *utf = 0;
            break;
        }
        *utf++ = *ucs++;
        ucs++;
    }
}

static uint8_t
msc_fat_view_short_name_checksum(const char *short_name)
{
    uint8_t check_sum = 0;
    int i;

    for (i = 0; i < 11; ++i) {
        check_sum = (((check_sum & 1) ? 0x80 : 0) | (check_sum >> 1)) + *short_name++;
    }

    return check_sum;
}

/*
 * Return pointer to character in the string if offset is valid
 * If offset is outside of the string return NULL
 */
static const char *
long_name_part(const char *long_name, int long_name_len, int long_name_offset)
{
    if (long_name_offset > long_name_len + 1) {
        return NULL;
    }
    return long_name + long_name_offset;
}

void
msc_fat_view_write_long_name_entry(struct out_stream *ostr, const char *name,
                                   const char *short_name)
{
    int len;
    int n;
    int i;
    uint8_t checksum;
    int offset;

    len = strlen(name);
    if (len) {
        checksum = msc_fat_view_short_name_checksum(short_name);
        n = (len + 12) / 13;
        for (i = n; i > 0; --i) {
            offset = (i - 1) * 13;
            /* First part of long entry has the highest index and 6th bit set */
            ostream_write_uint8(ostr, i + ((i == n) ? 0x40 : 0));
            msc_fat_view_write_ucs_2(ostr, long_name_part(name, len, offset), 5);
            /* attrib */
            ostream_write_uint8(ostr, 0x0f);
            /* reserved2 */
            ostream_write_uint8(ostr, 0);
            ostream_write_uint8(ostr, checksum);
            msc_fat_view_write_ucs_2(ostr, long_name_part(name, len, offset + 5), 6);
            /* reserved3 */
            ostream_write_uint16(ostr, 0);
            msc_fat_view_write_ucs_2(ostr, long_name_part(name, len, offset + 11), 2);
        }
    }
}

static char *
msc_fat_view_create_short_name(const dir_entry_t *entry, char short_name[11])
{
    int i, j;
    int len = strlen(entry->file->name);
    int last_dot;
    const char *name = entry->file->name;
    bool add_tilde = false;

    memset(short_name, ' ', 11);
    if (entry->dir_slots > 1) {
        last_dot = len;
        for (i = len - 1; i > 0; --i) {
            if (name[i] == '.') {
                last_dot = i++;
                for (j = 8; j < 11 && i < len; ++i, ++j) {
                    short_name[j] = toupper((uint8_t)name[i]);
                }
                break;
            }
        }
        for (i = 0, j = 0; j < 8 && i < last_dot; ++i) {
            if (name[i] != '.' && name[i] != ' ') {
                short_name[j++] = toupper((uint8_t)name[i]);
            } else {
                add_tilde = true;
            }
        }
        if (add_tilde) {
            for (i = 0; i < 6 && short_name[i] != ' '; ++i) {
            }
            short_name[i++] = '~';
            /* Not name should be checked for duplicates */
            short_name[i] = '1';
        }
    } else {
        for (j = 0, i = 0; i < len; ++i) {
            if (name[i] != '.') {
                short_name[j++] = name[i];
            } else {
                j = 8;
            }
        }
    }
    return short_name;
}

static void
msc_fat_view_read_boot_sector(uint8_t buffer[512])
{
    memcpy(buffer, &boot_sector_start, sizeof(boot_sector_start));
    memset(buffer + sizeof(boot_sector_start), 0, 512 - 2 - sizeof(boot_sector_start));
    buffer[510] = 0x55;
    buffer[511] = 0xAA;
}

static void
msc_fat_view_read_fat_sector(uint16_t fat_sector, uint8_t buffer[512])
{
    uint32_t cluster;
    uint32_t cluster_bits;
    /* First bit of fat_sector is sector_start_bit in total fat bits */
    uint32_t sector_start_bit;
    uint32_t sector_limit_bit;
    fat_chain_t *cache = NULL;
    cluster_t next_cluster;

    sector_start_bit = fat_sector * SECTOR_BIT_COUNT;
    sector_limit_bit = sector_start_bit + SECTOR_BIT_COUNT;
    if (fat_sector == 0) {
        /* Fill first FAT_ENTRY_SIZE bits of fat with signature 0x(FFFFF)FF8 */
        msc_fat_view_put_cluster_bits(buffer, sector_start_bit, 0, 0xFFFFFFF8);
        /* Fill second FAT_ENTRY_SIZE bits of fat with signature 0x(FFFFF)FFF */
        msc_fat_view_put_cluster_bits(buffer, sector_start_bit, FAT_ENTRY_SIZE, 0xFFFFFFFF);
        cluster_bits = 2 * FAT_ENTRY_SIZE;
        cluster = 2;
    } else {
        /* First bits of this fat_sectors belong to this cluster */
        cluster = sector_start_bit / FAT_ENTRY_SIZE;
        /* cluster start bit (can be less than sector_start_bit for FAT12) */
        cluster_bits = cluster * FAT_ENTRY_SIZE;
    }

    for (; cluster_bits < sector_limit_bit; cluster_bits += FAT_ENTRY_SIZE, ++cluster) {
        next_cluster = msc_fat_view_fat_next_cluster(cluster, &cache);
        msc_fat_view_put_cluster_bits(buffer, sector_start_bit, cluster_bits, next_cluster);
    }
}

static void
msc_fat_view_read_root_sector(uint16_t dir_sector, uint8_t buffer[512])
{
    int i;
    int cluster = 2;
    int cluster_count;
    uint32_t size;
    struct mem_out_stream mstr;
    struct out_stream *ostr = (struct out_stream *)&mstr;
    fat_dir_entry_t fat_dir_entry;
    dir_entry_t *entry;
    char short_name[11];

    memset(buffer, 0, 512);
    mem_ostream_init(&mstr, buffer, 512);
    mstr.write_ptr -= dir_sector * 512;

    TU_LOG1("msc_fat_view_read_root %d\n", dir_sector);
    for (i = 0; i < root_dir_entry_count; ++i) {
        entry = &root_dir[i];
        msc_fat_view_create_short_name(entry, short_name);
        if (entry->dir_slots > 1) {
            msc_fat_view_write_long_name_entry(ostr, entry->file->name, short_name);
        }
        memcpy(fat_dir_entry.bytes, short_name, 11);
        /* Clear out rest of the entry */
        memset(fat_dir_entry.bytes + 11, 0, 32 - 11);

        fat_dir_entry.attr = root_dir[i].file->attributes;
        if (i > 0) {
            assert(root_dir[i].file->size);
            size = root_dir[i].file->size(root_dir[i].file);
            if (size) {
                cluster_count = cluster_count_from_bytes(size);
                fat_dir_entry.cluster_hi = htole16((cluster >> 16));
                fat_dir_entry.cluster_lo = htole16((uint16_t)cluster);
                fat_dir_entry.size = htole32(size);
                cluster += cluster_count;
            }
        }
        ostream_write(ostr, fat_dir_entry.bytes, 32, false);
    }
}

static void
msc_fat_view_read_sector(uint32_t sector, uint8_t buffer[512])
{
    cluster_t cluster;
    uint32_t sector_in_cluster;
    cluster_t cluster_in_chain;
    dir_entry_t *dir_entry;

    if (sector == 0) {
        msc_fat_view_read_boot_sector(buffer);
    } else if (sector < FAT_ROOT_DIR_FIRST_SECTOR) {
        msc_fat_view_read_fat_sector(sector - 1, buffer);
    } else if (sector < FAT_CLUSTER2_FIRST_SECTOR) {
        msc_fat_view_read_root_sector(sector - FAT_SECTOR_COUNT - 1, buffer);
    } else {
        sector_to_cluster(sector, &cluster, &sector_in_cluster);
        dir_entry = msc_fat_view_dir_entry_from_cluster(cluster, &cluster_in_chain);
        if (dir_entry) {
            dir_entry->file->read_sector(dir_entry->file, sector_in_cluster + cluster_in_chain * SECTORS_PER_CLUSTER,
                                         buffer);
        } else {
            memset(buffer, 0, 512);
        }
    }
}

static int32_t
msc_fat_view_write_fat_sector(uint32_t fat_sector, const uint8_t *buffer)
{
    uint32_t sector_start_bit;
    uint32_t sector_limit_bit;
    uint32_t cluster_bits;
    fat_chain_t *cache = NULL;
    cluster_t cluster;
    cluster_t next_cluster;

    sector_start_bit = fat_sector * SECTOR_BIT_COUNT;
    sector_limit_bit = sector_start_bit + SECTOR_BIT_COUNT;
    if (fat_sector == 0) {
        cluster_bits = 2 * FAT_ENTRY_SIZE;
        cluster = 2;
    } else {
        /* First bits of this fat_sectors belong to this cluster */
        cluster = sector_start_bit / FAT_ENTRY_SIZE;
        /* cluster start bit (can be less than sector_start_bit for FAT12) */
        cluster_bits = cluster * FAT_ENTRY_SIZE;
    }

    for (; cluster_bits < sector_limit_bit; cluster_bits += FAT_ENTRY_SIZE, ++cluster) {
        next_cluster = msc_fat_view_get_cluster_bits(buffer, sector_start_bit, cluster_bits);
        fat_modify_next_cluster(cluster, next_cluster, &cache);
    }

    return 512;
}

msc_fat_view_write_handler_t *current_write_handler;

static void
msc_fat_view_handle_new_file(const fat_dir_entry_t *entry, const char *name)
{
    cluster_t cluster;
    uint32_t sector;
    fat_chain_t *chain;
    int rc;

    MSC_FAT_VIEW_LOG_INFO("Handle new file %s %d %d\n", name,
                          (int)entry->cluster_lo, (int)entry->size);
    /* new entry with start cluster and file size */
    if (entry->cluster_lo != 0 && entry->size > 0) {
        cluster = le16toh(entry->cluster_lo);
        sector = cluster_to_sector(cluster);
        chain = fat_chain_find(cluster);
        if (current_write_handler) {
            rc = current_write_handler->file_written(current_write_handler, entry->size,
                                                     sector, chain && chain->first == cluster);
            if (rc) {
                current_write_handler = NULL;
            }
        }
    }
}

static int32_t
msc_fat_view_write_root_sector(uint32_t sector, const uint8_t *buffer)
{
    const fat_dir_entry_t *entry = (const fat_dir_entry_t *)buffer;
    const fat_dir_entry_t *limit = (const fat_dir_entry_t *)(buffer + SECTOR_SIZE);
    dir_entry_t *file_entry;
    char name[79];
    int i, j;
    int n;
    uint16_t checksum = 0xFFFF;
    int32_t res = 512;

    MSC_FAT_VIEW_LOG_INFO("Write root dir sector %d\n", sector);

    for (i = 0; i < root_dir_entry_count; ++i) {
        if (root_dir[i].deleted == 0) {
            root_dir[i].deleted = 1;
        }
    }

    for (; entry < limit; ++entry) {
        if (entry->bytes[0] == 0xe5) {
            continue;
        }
        if (entry->attr == 0xF) {
            /* Long file name */
            if (entry->sequence & 0x40) {
                n = (entry->sequence & 0xF);
                checksum = entry->checksum;

                while (n) {
                    n--;
                    assert(entry->checksum == checksum);
                    /* If name in directory is longer then we have space for just ignore it */
                    if (n > sizeof(name) / 13) {
                        continue;
                    } else if (n == sizeof(name) / 13) {
                        /* This part also needs to be ignored, but store \0 for strcmp() sake */
                        name[sizeof(name) / 13] = 0;
                    }
                    msc_fat_view_read_ucs_2(name + n * 13, (uint8_t *)entry->name1, 5);
                    msc_fat_view_read_ucs_2(name + n * 13 + 5, (uint8_t *)entry->name2, 6);
                    msc_fat_view_read_ucs_2(name + n * 13 + 5 + 6, (uint8_t *)entry->name3, 2);
                    entry++;
                }
                entry--;
                continue;
            }
        }
        /* Exclude directories and volume names */
        if (msc_fat_view_short_name_checksum(entry->name) != checksum) {
            for (i = 0; i < 8 && entry->name[i] != ' '; ++i) {
                name[i] = entry->name[i];
            }
            for (j = 0; j < 3 && entry->ext[j] != ' '; ++j) {
                if (j == 0) {
                    name[i++] = '.';
                }
                name[i++] = entry->ext[j];
            }
            name[i] = '\0';
        }
        MSC_FAT_VIEW_LOG_DEBUG("File name %s\n", name);
        file_entry = msc_fat_view_find_dir_entry(name);
        if (file_entry == NULL) {
            if ((entry->attr & (FAT_FILE_ENTRY_ATTRIBUTE_DIRECTORY | FAT_FILE_ENTRY_ATTRIBUTE_LABEL)) == 0) {
                /* new file just showed up in root directory */
                msc_fat_view_handle_new_file(entry, name);
            }
        } else {
            file_entry->deleted = 0;
        }
    }
    for (file_entry = root_dir; file_entry < root_dir + root_dir_entry_count; ++file_entry) {
        MSC_FAT_VIEW_LOG_INFO("%s %d\n", file_entry->file->name, file_entry->deleted);
        if (file_entry->deleted == 1) {
            file_entry->deleted = 2;
            if (file_entry->file && file_entry->file->delete_entry) {
                MSC_FAT_VIEW_LOG_INFO("Deleted entry %s\n", file_entry->file->name);
                file_entry->file->delete_entry(file_entry->file);
            }
        }
    }

    return res;
}

static int
msc_fat_view_write_unallocated_sector(uint32_t sector, uint8_t *buffer)
{
    int res;

    msc_fat_view_write_handler_t *write_handler = current_write_handler;

    if (write_handler) {
        res = write_handler->write_sector(write_handler, sector, buffer);
        if (res == 512) {
            return 512;
        }
    }
    LINK_TABLE_FOREACH(p, msc_fat_view_write_handlers) {
        if (*p == current_write_handler) {
            continue;
        }
        write_handler = *p;
        res = write_handler->write_sector(write_handler, sector, buffer);
        if (res == 512) {
            current_write_handler = write_handler;
            return 512;
        }
    }
    current_write_handler = NULL;

    return 512;
}

static int
msc_fat_view_write_file_sector(dir_entry_t *dir_entry, uint32_t file_sector, uint8_t *buffer)
{
    if (dir_entry->file->write_sector) {
        dir_entry->file->write_sector(dir_entry->file, file_sector, buffer);
    }
    return 512;
}

static int
msc_fat_view_write_normal_sector(uint32_t sector, uint8_t *buffer)
{
    cluster_t cluster;
    cluster_t cluster_in_chain;
    uint32_t sector_in_cluster;
    dir_entry_t *dir_entry;
    int res;

    sector_to_cluster(sector, &cluster, &sector_in_cluster);
    dir_entry = msc_fat_view_dir_entry_from_cluster(cluster, &cluster_in_chain);
    if (dir_entry == NULL) {
        res = msc_fat_view_write_unallocated_sector(sector, buffer);
    } else {
        res = msc_fat_view_write_file_sector(dir_entry, sector_in_cluster + cluster_in_chain * SECTORS_PER_CLUSTER,
                                             buffer);
    }

    return res;
}

static void
add_dir_entry(file_entry_t *const *entry)
{
    if ((*entry)->valid == NULL || (*entry)->valid(*entry) == MSC_FAT_VIEW_FILE_ENTRY_VALID) {
        msc_fat_view_add_dir_entry(*entry);
    }
}

static void
init_disk_data(void)
{
    root_dir_entry_count = 0;

    msc_fat_view_add_dir_entry(&volume_label);
    if (MYNEWT_VAL(MSC_FAT_VIEW_SYSTEM_VOLUME_INFORMATION)) {
        msc_fat_view_add_dir_entry(&system_volume_information);
    }

    if (MYNEWT_VAL(MSC_FAT_VIEW_DROP_IMAGE_HERE)) {
        msc_fat_view_add_dir_entry(&drop_image_here);
    }
    LINK_TABLE_FOREACH_CALL(msc_fat_view_root_entry, add_dir_entry);

    if (MYNEWT_VAL(MSC_FAT_VIEW_COREDUMP_FILES)) {
        msc_fat_view_add_coredumps();
    }
}

static void
update_disk_data(void)
{
    int i;

    free_clusters = (cluster_t)((SECTOR_COUNT - FAT_CLUSTER2_FIRST_SECTOR) / SECTORS_PER_CLUSTER);
    fat_chain_count = 0;

    for (i = 0; i < root_dir_entry_count; ++i) {
        msc_fat_view_update_dir_entry(i);
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

    MSC_FAT_VIEW_LOG_INFO("SCSI inquiry\n");

    last_scsi_command = SCSI_CMD_INQUIRY;

    memcpy(vendor_id, vid, strlen(vid) + 1);
    memcpy(product_id, pid, strlen(pid) + 1);
    memcpy(product_rev, rev, strlen(rev) + 1);
}

/*
 * Invoked when received Test Unit Ready command.
 * return true allowing host to read/write this LUN e.g SD card inserted
 */
bool
tud_msc_test_unit_ready_cb(uint8_t lun)
{
    bool ret = medium_state == MEDIUM_PRESENT;

    if (medium_state == MEDIUM_RELOAD) {
        /* This path will report medium not present */
        medium_state = REPORT_MEDIUM_CHANGE;
        update_disk_data();
    } else if (medium_state == REPORT_MEDIUM_CHANGE) {
        /* This will report medium change notification */
        tud_msc_set_sense(lun, SCSI_SENSE_UNIT_ATTENTION, 0x28, 0);
        medium_state = MEDIUM_PRESENT;
    }

    last_scsi_command = SCSI_CMD_TEST_UNIT_READY;
    return ret;
}

/*
 * Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
 * Application update block count and block size
 */
void
tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
    (void)lun;

    last_scsi_command = SCSI_CMD_READ_CAPACITY_10;

    if (medium_state < MEDIUM_RELOAD) {
        *block_count = 0;
        *block_size = 0;
    } else {
        *block_count = SECTOR_COUNT;
        *block_size = SECTOR_SIZE;
    }
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

    last_scsi_command = SCSI_CMD_START_STOP_UNIT;

    if (load_eject) {
        if (start) {
            medium_state = MEDIUM_PRESENT;
        } else {
            medium_state = MEDIUM_NOT_PRESENT;
        }
    }

    return true;
}

/*
 * Callback invoked when received READ10 command.
 * Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
 */
int32_t
tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
    (void)lun;
    (void)offset;

    last_scsi_command = SCSI_CMD_READ_10;

    if (medium_state < MEDIUM_RELOAD) {
        return -1;
    }
    msc_fat_view_read_sector(lba, buffer);

    return bufsize;
}

/*
 * Callback invoked when received WRITE10 command.
 * Process data in buffer to disk's storage and return number of written bytes
 */
int32_t
tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
    int32_t res;
    MSC_FAT_VIEW_LOG_DEBUG("SCSI WRITE10 %d, %d, %d\n", (int)lba, (int)offset, (int)bufsize);
    assert(bufsize == SECTOR_SIZE);
    assert(offset == 0);

    last_scsi_command = SCSI_CMD_WRITE_10;

    (void)lun;

    if (medium_state < MEDIUM_RELOAD) {
        return -1;
    }

    if (lba == 0) {
        res = bufsize;
        /* Ignore writes to boot sector */
    } else if (lba < FAT_ROOT_DIR_FIRST_SECTOR) {
        res = msc_fat_view_write_fat_sector(lba - FAT_FIRST_SECTOR, buffer);
    } else if (lba < FAT_CLUSTER2_FIRST_SECTOR) {
        res = msc_fat_view_write_root_sector(lba - FAT_ROOT_DIR_FIRST_SECTOR, buffer);
    } else {
        res = msc_fat_view_write_normal_sector(lba, buffer);
    }

    return res;
}

/*
 * Callback invoked when received an SCSI command not in built-in list below
 * - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
 * - READ10 and WRITE10 has their own callbacks
 */
int32_t
tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize)
{
    void const *response = NULL;
    int32_t resplen = 0;

    /* most scsi handled is input */
    bool in_xfer = true;

    last_scsi_command = scsi_cmd[0];

    MSC_FAT_VIEW_LOG_INFO("SCSI cmd 0x%02X\n", scsi_cmd[0]);

    switch (scsi_cmd[0]) {
    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
        /* Host is about to read/write etc ... better not to disconnect disk */
        resplen = -1;
        break;

    default:
        /* Set Sense = Invalid Command Operation */
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

        /* negative means error -> tinyusb could stall and/or response with failed status */
        resplen = -1;
        break;
    }

    /* return resplen must not larger than bufsize */
    if (resplen > bufsize) {
        resplen = bufsize;
    }

    if (response && (resplen > 0)) {
        if (in_xfer) {
            memcpy(buffer, response, resplen);
        } else {
            /* SCSI output */
        }
    }

    return resplen;
}

void
msc_fat_view_media_eject(void)
{
    medium_state = MEDIUM_NOT_PRESENT;
}

void
msc_fat_view_media_insert(void)
{
    medium_state = MEDIUM_RELOAD;
}

void
msc_fat_view_pkg_init(void)
{
    if (MYNEWT_VAL(MSC_FAT_VIEW_AUTO_INSERT)) {
        msc_fat_view_media_insert();
    }
    init_disk_data();
}

#if MYNEWT_VAL(MSC_FAT_BOOT_PIN) >= 0
void
boot_preboot(void)
{
    hal_gpio_init_in(MYNEWT_VAL(MSC_FAT_BOOT_PIN), (hal_gpio_pull_t)MYNEWT_VAL(MSC_FAT_BOOT_PIN_PULL));
    os_cputime_delay_usecs(30);
    if (hal_gpio_read(MYNEWT_VAL(MSC_FAT_BOOT_PIN)) == MYNEWT_VAL(MSC_FAT_BOOT_PIN_VALUE)) {
        hal_gpio_deinit(MYNEWT_VAL(MSC_FAT_BOOT_PIN));
#if MYNEWT_VAL(MSC_FAT_VIEW_COREDUMP_FILES)
        extern void msc_fat_view_coredump_pkg_init(void);
        msc_fat_view_coredump_pkg_init();
#endif
        msc_fat_view_pkg_init();
        tinyusb_start();
    }
    hal_gpio_deinit(MYNEWT_VAL(MSC_FAT_BOOT_PIN));
}
#endif
