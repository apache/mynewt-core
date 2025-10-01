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

#ifndef __DISK_H__
#define __DISK_H__

#include <stddef.h>
#include <inttypes.h>
#include <os/mynewt.h>
#include <os/link_tables.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DISK_EOK          0  /* Success */
#define DISK_EHW          1  /* Error accessing storage medium */
#define DISK_ENOMEM       2  /* Insufficient memory */
#define DISK_ENOENT       3  /* No such file or directory */
#define DISK_EOS          4  /* OS error */
#define DISK_EUNINIT      5  /* File system not initialized */

typedef struct disk disk_t;
typedef struct disk_info disk_info_t;
typedef struct disk_listener disk_listener_t;

/**
 * Disk information
 */
struct disk_info {
    /* Disk name (information only) */
    const char *name;
    /* Disk block (sector) count */
    uint32_t block_count;
    /* Block size (usually 512) */
    uint16_t block_size;
    /* Set to 1 if disk is present */
    uint16_t present : 1;
};

typedef struct disk_ops {
    /** Get basic information about disk
     *
     * @param disk - disk to query
     * @param info - disk information to fill
     * @return 0 on success, SYS_EINVAL if arguments are incorrect
     */
    int (*get_info)(const struct disk *disk, struct disk_info *info);

    /**
     * Inform driver that disk was just removed
     * @param disk - disk that was ejected
     * @return 0 on success, SYS_EINVAL if arguments are incorrect
     */
    int (*eject)(struct disk *disk);
    /**
     * Read disk block
     * @param disk - dist to read from
     * @param lba - block to read
     * @param buf - buffer for data
     * @param block_count - number for blocks to read
     * @return 0 on success
     */
    int (*read)(struct disk *disk, uint32_t lba, void *buf, uint32_t block_count);
    /**
     * Write disk block
     * @param disk - dist to write to
     * @param lba - block to write to
     * @param buf - data buffer
     * @param block_count - number of blocks to write
     * @return 0 on success
     */
    int (*write)(struct disk *disk, uint32_t lba, const void *buf, uint32_t block_count);
} disk_ops_t;

/* Disk listener operations */
typedef struct disk_listener_ops {
    /* New disk was inserted to driver and could be mounter */
    int (*disk_added)(struct disk_listener *listener, struct disk *disk);
    /* Disk was ejected and should be unmounted */
    int (*disk_removed)(struct disk_listener *listener, struct disk *disk);
} disk_listener_ops_t;

/* Disk listener */
struct disk_listener {
    const disk_listener_ops_t *ops;
};

/* Base for disks */
struct disk {
    const disk_ops_t *ops;
};

/**
 * Function called when disk was inserted into a driver
 * and system could mount it or otherwise be aware that new
 * disk could be used
 *
 * @param disk - disk that was inserted
 *
 * @return 0 - for success
 */
int mn_disk_inserted(disk_t *disk);

/**
 * Function called when disk was removed, if disk was
 * mounted it file system can be unmounted. If disk
 * was present to MSC it can no longer be advertised
 * as present.
 *
 * @param disk - disk that was remove from driver
 *
 * @return 0 -for success
 */
int mn_disk_ejected(disk_t *disk);

/**
 * Function return 1 if disk is present
 *
 * @param disk - disk to check for presence
 * @return 1 - disk is present, 0 - disk is not present
 */
static inline int disk_present(disk_t *disk)
{
    disk_info_t di;
    disk->ops->get_info(disk, &di);

    return di.present;
}

/**
 * Get disk information
 * Disk information consist of size information
 *
 * @param disk - disk to get information from
 * @param di - pointer to data to received disk information
 *
 * @return 0 - for success
 */
static inline int mn_disk_info_get(disk_t *disk, disk_info_t *di)
{
    return disk->ops->get_info(disk, di);
}

/**
 * Read sector(s) from disk
 *
 * @param disk - disk to read data from
 * @param lba - sector number
 * @param buf - buffer to be filled with data from disk
 * @param block_count - number of sectors to read
 * @return 0 - for success
 */
static inline int mn_disk_read(disk_t *disk, uint32_t lba, void *buf, uint32_t block_count)
{
    return disk->ops->read(disk, lba, buf, block_count);
}

/**
 * Write sector(s) to disk
 *
 * @param disk - disk to write to
 * @param lba - first sector to write to
 * @param buf - buffer with data
 * @param block_count - number of sectors to write
 * @return 0 - for success
 */
static inline int mn_disk_write(disk_t *disk, uint32_t lba, const void *buf, uint32_t block_count)
{
    return disk->ops->write(disk, lba, buf, block_count);
}

/**
 * Eject disk
 *
 * Calling this function will result in calling disk listeners about
 * disk being removed via disk_removed(). This may result in file
 * system being unmounted.
 *
 * @param disk - disk to eject
 * @return 0 - for success
 */
static inline int mn_disk_eject(disk_t *disk)
{
    return disk->ops->eject(disk);
}

/**
 * Link time table to hold all available disk listeners
 * that will be informed when disk is available and
 * can be mounted or used in other ways.
 */
LINK_TABLE(disk_listener_t *, disk_listeners)

/**
 * @brief Macros to put disk_listener in link table
 *
 * i.e.
 * // Define disk_listener_t structure
 * disk_listener_t mounter = {
 *    .opt = &mounter_ops,
 * };
 *
 * DISK_LISTENER(mounter_entry, mounter)
 *
 * @param listener_entry - new pointer variable name that will be put in disk_listeners table
 * @param listener - object of type disk_listener_t
 */
#define DISK_LISTENER(listener_entry, listener) \
    LINK_TABLE_ELEMENT(disk_listeners, listener_entry) = &listener;

#ifdef __cplusplus
}
#endif

#endif
