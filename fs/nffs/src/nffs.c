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

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "sysinit/sysinit.h"
#include "sysflash/sysflash.h"
#include "bsp/bsp.h"
#include "hal/hal_flash.h"
#include "flash_map/flash_map.h"
#include "os/os_mempool.h"
#include "os/os_mutex.h"
#include "os/os_malloc.h"
#include "stats/stats.h"
#include "nffs_priv.h"
#include "nffs/nffs.h"
#include "fs/fs_if.h"
#include "disk/disk.h"

struct nffs_area *nffs_areas;
uint8_t nffs_num_areas;
uint8_t nffs_scratch_area_idx;
uint16_t nffs_block_max_data_sz;
struct nffs_area_desc *nffs_current_area_descs;

struct os_mempool nffs_file_pool;
struct os_mempool nffs_dir_pool;
struct os_mempool nffs_inode_entry_pool;
struct os_mempool nffs_block_entry_pool;
struct os_mempool nffs_cache_inode_pool;
struct os_mempool nffs_cache_block_pool;

void *nffs_file_mem;
void *nffs_inode_mem;
void *nffs_block_entry_mem;
void *nffs_cache_inode_mem;
void *nffs_cache_block_mem;
void *nffs_dir_mem;

struct nffs_inode_entry *nffs_root_dir;
struct nffs_inode_entry *nffs_lost_found_dir;

static struct os_mutex nffs_mutex;

struct log nffs_log;

static int nffs_open(const char *path, uint8_t access_flags,
  struct fs_file **out_file);
static int nffs_close(struct fs_file *fs_file);
static int nffs_read(struct fs_file *fs_file, uint32_t len, void *out_data,
  uint32_t *out_len);
static int nffs_write(struct fs_file *fs_file, const void *data, int len);
static int nffs_seek(struct fs_file *fs_file, uint32_t offset);
static uint32_t nffs_getpos(const struct fs_file *fs_file);
static int nffs_file_len(const struct fs_file *fs_file, uint32_t *out_len);
static int nffs_unlink(const char *path);
static int nffs_rename(const char *from, const char *to);
static int nffs_mkdir(const char *path);
static int nffs_opendir(const char *path, struct fs_dir **out_fs_dir);
static int nffs_readdir(struct fs_dir *dir, struct fs_dirent **out_dirent);
static int nffs_closedir(struct fs_dir *dir);
static int nffs_dirent_name(const struct fs_dirent *fs_dirent, size_t max_len,
  char *out_name, uint8_t *out_name_len);
static int nffs_dirent_is_dir(const struct fs_dirent *fs_dirent);

struct fs_ops nffs_ops = {
    .f_open = nffs_open,
    .f_close = nffs_close,
    .f_read = nffs_read,
    .f_write = nffs_write,

    .f_seek = nffs_seek,
    .f_getpos = nffs_getpos,
    .f_filelen = nffs_file_len,

    .f_unlink = nffs_unlink,
    .f_rename = nffs_rename,
    .f_mkdir = nffs_mkdir,

    .f_opendir = nffs_opendir,
    .f_readdir = nffs_readdir,
    .f_closedir = nffs_closedir,

    .f_dirent_name = nffs_dirent_name,
    .f_dirent_is_dir = nffs_dirent_is_dir,

    .f_name = "nffs"
};

STATS_SECT_DECL(nffs_stats) nffs_stats;
STATS_NAME_START(nffs_stats)
    STATS_NAME(nffs_stats, nffs_hashcnt_ins)
    STATS_NAME(nffs_stats, nffs_hashcnt_rm)
    STATS_NAME(nffs_stats, nffs_object_count)
    STATS_NAME(nffs_stats, nffs_iocnt_read)
    STATS_NAME(nffs_stats, nffs_iocnt_write)
    STATS_NAME(nffs_stats, nffs_gccnt)
    STATS_NAME(nffs_stats, nffs_readcnt_data)
    STATS_NAME(nffs_stats, nffs_readcnt_block)
    STATS_NAME(nffs_stats, nffs_readcnt_crc)
    STATS_NAME(nffs_stats, nffs_readcnt_copy)
    STATS_NAME(nffs_stats, nffs_readcnt_format)
    STATS_NAME(nffs_stats, nffs_readcnt_gccollate)
    STATS_NAME(nffs_stats, nffs_readcnt_inode)
    STATS_NAME(nffs_stats, nffs_readcnt_inodeent)
    STATS_NAME(nffs_stats, nffs_readcnt_rename)
    STATS_NAME(nffs_stats, nffs_readcnt_update)
    STATS_NAME(nffs_stats, nffs_readcnt_filename)
    STATS_NAME(nffs_stats, nffs_readcnt_object)
    STATS_NAME(nffs_stats, nffs_readcnt_detect)
STATS_NAME_END(nffs_stats)

static void
nffs_lock(void)
{
    int rc;

    rc = os_mutex_pend(&nffs_mutex, 0xffffffff);
    assert(rc == 0 || rc == OS_NOT_STARTED);
}

static void
nffs_unlock(void)
{
    int rc;

    rc = os_mutex_release(&nffs_mutex);
    assert(rc == 0 || rc == OS_NOT_STARTED);
}

static int
nffs_stats_init(void)
{
    int rc;

    rc = stats_init_and_reg(
                    STATS_HDR(nffs_stats),
                    STATS_SIZE_INIT_PARMS(nffs_stats, STATS_SIZE_32),
                    STATS_NAME_INIT_PARMS(nffs_stats),
                    "nffs_stats");
    if (rc) {
        if (rc < 0) {
            /* multiple initializations are okay */
            rc = 0;
        } else {
            rc = FS_EOS;
        }
    }
    return rc;
}

/**
 * Opens a file at the specified path.  The result of opening a nonexistent
 * file depends on the access flags specified.  All intermediate directories
 * must already exist.
 *
 * The mode strings passed to fopen() map to nffs_open()'s access flags as
 * follows:
 *   "r"  -  FS_ACCESS_READ
 *   "r+" -  FS_ACCESS_READ | FS_ACCESS_WRITE
 *   "w"  -  FS_ACCESS_WRITE | FS_ACCESS_TRUNCATE
 *   "w+" -  FS_ACCESS_READ | FS_ACCESS_WRITE | FS_ACCESS_TRUNCATE
 *   "a"  -  FS_ACCESS_WRITE | FS_ACCESS_APPEND
 *   "a+" -  FS_ACCESS_READ | FS_ACCESS_WRITE | FS_ACCESS_APPEND
 *
 * @param path              The path of the file to open.
 * @param access_flags      Flags controlling file access; see above table.
 * @param out_file          On success, a pointer to the newly-created file
 *                              handle gets written here.
 *
 * @return                  0 on success; nonzero on failure.
 */
static int
nffs_open(const char *path, uint8_t access_flags, struct fs_file **out_fs_file)
{
    int rc;
    struct nffs_file *out_file;
    char *filepath = NULL;

    nffs_lock();

    if (!nffs_misc_ready()) {
        rc = FS_EUNINIT;
        goto done;
    }

    filepath = disk_filepath_from_path(path);

    rc = nffs_file_open(&out_file, filepath, access_flags);
    if (rc != 0) {
        goto done;
    }
    *out_fs_file = (struct fs_file *)out_file;
done:
    if (filepath) {
        free(filepath);
    }
    nffs_unlock();
    if (rc != 0) {
        *out_fs_file = NULL;
    }
    return rc;
}

/**
 * Closes the specified file and invalidates the file handle.  If the file has
 * already been unlinked, and this is the last open handle to the file, this
 * operation causes the file to be deleted from disk.
 *
 * @param file              The file handle to close.
 *
 * @return                  0 on success; nonzero on failure.
 */
static int
nffs_close(struct fs_file *fs_file)
{
    int rc;
    struct nffs_file *file = (struct nffs_file *)fs_file;

    if (file == NULL) {
        return 0;
    }

    nffs_lock();
    rc = nffs_file_close(file);
    nffs_unlock();

    return rc;
}

/**
 * Positions a file's read and write pointer at the specified offset.  The
 * offset is expressed as the number of bytes from the start of the file (i.e.,
 * seeking to offset 0 places the pointer at the first byte in the file).
 *
 * @param file              The file to reposition.
 * @param offset            The 0-based file offset to seek to.
 *
 * @return                  0 on success; nonzero on failure.
 */
static int
nffs_seek(struct fs_file *fs_file, uint32_t offset)
{
    int rc;
    struct nffs_file *file = (struct nffs_file *)fs_file;

    nffs_lock();
    rc = nffs_file_seek(file, offset);
    nffs_unlock();

    return rc;
}

/**
 * Retrieves the current read and write position of the specified open file.
 *
 * @param file              The file to query.
 *
 * @return                  The file offset, in bytes.
 */
static uint32_t
nffs_getpos(const struct fs_file *fs_file)
{
    uint32_t offset;
    const struct nffs_file *file = (const struct nffs_file *)fs_file;

    nffs_lock();
    offset = file->nf_offset;
    nffs_unlock();

    return offset;
}

/**
 * Retrieves the current length of the specified open file.
 *
 * @param file              The file to query.
 * @param out_len           On success, the number of bytes in the file gets
 *                              written here.
 *
 * @return                  0 on success; nonzero on failure.
 */
static int
nffs_file_len(const struct fs_file *fs_file, uint32_t *out_len)
{
    int rc;
    const struct nffs_file *file = (const struct nffs_file *)fs_file;

    nffs_lock();
    rc = nffs_inode_data_len(file->nf_inode_entry, out_len);
    nffs_unlock();

    return rc;
}

/**
 * Reads data from the specified file.  If more data is requested than remains
 * in the file, all available data is retrieved and a success code is returned.
 *
 * @param file              The file to read from.
 * @param len               The number of bytes to attempt to read.
 * @param out_data          The destination buffer to read into.
 * @param out_len           On success, the number of bytes actually read gets
 *                              written here.  Pass null if you don't care.
 *
 * @return                  0 on success; nonzero on failure.
 */
static int
nffs_read(struct fs_file *fs_file, uint32_t len, void *out_data,
          uint32_t *out_len)
{
    int rc;
    struct nffs_file *file = (struct nffs_file *)fs_file;

    nffs_lock();
    rc = nffs_file_read(file, len, out_data, out_len);
    nffs_unlock();

    return rc;
}

/**
 * Writes the supplied data to the current offset of the specified file handle.
 *
 * @param file              The file to write to.
 * @param data              The data to write.
 * @param len               The number of bytes to write.
 *
 * @return                  0 on success; nonzero on failure.
 */
static int
nffs_write(struct fs_file *fs_file, const void *data, int len)
{
    int rc;
    struct nffs_file *file = (struct nffs_file *)fs_file;

    nffs_lock();

    if (!nffs_misc_ready()) {
        rc = FS_EUNINIT;
        goto done;
    }

    rc = nffs_write_to_file(file, data, len);
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    nffs_unlock();
    return rc;
}

/**
 * Unlinks the file or directory at the specified path.  If the path refers to
 * a directory, all the directory's descendants are recursively unlinked.  Any
 * open file handles refering to an unlinked file remain valid, and can be
 * read from and written to.
 *
 * @path                    The path of the file or directory to unlink.
 *
 * @return                  0 on success; nonzero on failure.
 */
static int
nffs_unlink(const char *path)
{
    int rc;

    nffs_lock();

    if (!nffs_misc_ready()) {
        rc = FS_EUNINIT;
        goto done;
    }

    rc = nffs_path_unlink(path);
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    nffs_unlock();
    return rc;
}

/**
 * Performs a rename and / or move of the specified source path to the
 * specified destination.  The source path can refer to either a file or a
 * directory.  All intermediate directories in the destination path must
 * already exist.  If the source path refers to a file, the destination path
 * must contain a full filename path, rather than just the new parent
 * directory.  If an object already exists at the specified destination path,
 * this function causes it to be unlinked prior to the rename (i.e., the
 * destination gets clobbered).
 *
 * @param from              The source path.
 * @param to                The destination path.
 *
 * @return                  0 on success;
 *                          nonzero on failure.
 */
static int
nffs_rename(const char *from, const char *to)
{
    int rc;

    nffs_lock();

    if (!nffs_misc_ready()) {
        rc = FS_EUNINIT;
        goto done;
    }

    rc = nffs_path_rename(from, to);
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    nffs_unlock();
    return rc;
}

/**
 * Creates the directory represented by the specified path.  All intermediate
 * directories must already exist.  The specified path must start with a '/'
 * character.
 *
 * @param path                  The name of the directory to create.
 *
 * @return                      0 on success;
 *                              nonzero on failure.
 */
static int
nffs_mkdir(const char *path)
{
    int rc;

    nffs_lock();

    if (!nffs_misc_ready()) {
        rc = FS_EUNINIT;
        goto done;
    }

    rc = nffs_path_new_dir(path, NULL);
    if (rc != 0) {
        goto done;
    }

done:
    nffs_unlock();
    return rc;
}

/**
 * Opens the directory at the specified path.  The directory's contents can be
 * read with subsequent calls to nffs_readdir().  When you are done with the
 * directory handle, close it with nffs_closedir().
 *
 * Unlinking files from the directory while it is open may result in
 * unpredictable behavior.  New files can be created inside the directory.
 *
 * @param path                  The name of the directory to open.
 * @param out_dir               On success, points to the directory handle.
 *
 * @return                      0 on success;
 *                              FS_ENOENT if the specified directory does not
 *                                  exist;
 *                              other nonzero on error.
 */
static int
nffs_opendir(const char *path, struct fs_dir **out_fs_dir)
{
    int rc;
    struct nffs_dir **out_dir = (struct nffs_dir **)out_fs_dir;
    char *filepath = NULL;

    nffs_lock();

    if (!nffs_misc_ready()) {
        rc = FS_EUNINIT;
        goto done;
    }

    filepath = disk_filepath_from_path(path);

    rc = nffs_dir_open(filepath, out_dir);

done:
    if (filepath) {
        free(filepath);
    }
    nffs_unlock();
    if (rc != 0) {
        *out_dir = NULL;
    }
    return rc;
}

/**
 * Reads the next entry in an open directory.
 *
 * @param dir                   The directory handle to read from.
 * @param out_dirent            On success, points to the next child entry in
 *                                  the specified directory.
 *
 * @return                      0 on success;
 *                              FS_ENOENT if there are no more entries in the
 *                                  parent directory;
 *                              other nonzero on error.
 */
static int
nffs_readdir(struct fs_dir *fs_dir, struct fs_dirent **out_fs_dirent)
{
    int rc;
    struct nffs_dir *dir = (struct nffs_dir *)fs_dir;
    struct nffs_dirent **out_dirent = (struct nffs_dirent **)out_fs_dirent;

    nffs_lock();
    rc = nffs_dir_read(dir, out_dirent);
    nffs_unlock();

    return rc;
}

/**
 * Closes the specified directory handle.
 *
 * @param dir                   The name of the directory to close.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
nffs_closedir(struct fs_dir *fs_dir)
{
    int rc;
    struct nffs_dir *dir = (struct nffs_dir *)fs_dir;

    nffs_lock();
    rc = nffs_dir_close(dir);
    nffs_unlock();

    return rc;
}

/**
 * Retrieves the filename of the specified directory entry.  The retrieved
 * filename is always null-terminated.  To ensure enough space to hold the full
 * filename plus a null-termintor, a destination buffer of size
 * (NFFS_FILENAME_MAX_LEN + 1) should be used.
 *
 * @param dirent                The directory entry to query.
 * @param max_len               The size of the "out_name" character buffer.
 * @param out_name              On success, the entry's filename is written
 *                                  here; always null-terminated.
 * @param out_name_len          On success, contains the actual length of the
 *                                  filename, NOT including the
 *                                  null-terminator.
 *
 * @return                      0 on success; nonzero on failure.
 */
static int
nffs_dirent_name(const struct fs_dirent *fs_dirent, size_t max_len,
                 char *out_name, uint8_t *out_name_len)
{
    int rc;
    struct nffs_dirent *dirent = (struct nffs_dirent *)fs_dirent;

    nffs_lock();

    assert(dirent != NULL && dirent->nde_inode_entry != NULL);
    rc = nffs_inode_read_filename(dirent->nde_inode_entry, max_len, out_name,
                                  out_name_len);

    nffs_unlock();

    return rc;
}

/**
 * Tells you whether the specified directory entry is a sub-directory or a
 * regular file.
 *
 * @param dirent                The directory entry to query.
 *
 * @return                      1: The entry is a directory;
 *                              0: The entry is a regular file.
 */
static int
nffs_dirent_is_dir(const struct fs_dirent *fs_dirent)
{
    uint32_t id;
    const struct nffs_dirent *dirent = (const struct nffs_dirent *)fs_dirent;

    nffs_lock();

    assert(dirent != NULL && dirent->nde_inode_entry != NULL);
    id = dirent->nde_inode_entry->nie_hash_entry.nhe_id;

    nffs_unlock();

    return nffs_hash_id_is_dir(id);
}

/**
 * Erases all the specified areas and initializes them with a clean nffs
 * file system.
 *
 * @param area_descs        The set of areas to format.
 *
 * @return                  0 on success;
 *                          nonzero on failure.
 */
int
nffs_format(const struct nffs_area_desc *area_descs)
{
    int rc;

    nffs_lock();
    rc = nffs_format_full(area_descs);
    nffs_unlock();

    return rc;
}

/**
 * Searches for a valid nffs file system among the specified areas.  This
 * function succeeds if a file system is detected among any subset of the
 * supplied areas.  If the area set does not contain a valid file system,
 * a new one can be created via a separate call to nffs_format().
 *
 * @param area_descs        The area set to search.  This array must be
 *                              terminated with a 0-length area.
 *
 * @return                  0 on success;
 *                          FS_ECORRUPT if no valid file system was detected;
 *                          other nonzero on error.
 */
int
nffs_detect(const struct nffs_area_desc *area_descs)
{
    int rc;

    nffs_lock();
    rc = nffs_restore_full(area_descs);
    nffs_unlock();

    return rc;
}

/**
 * Initializes internal nffs memory and data structures.  This must be called
 * before any nffs operations are attempted.
 *
 * @return                  0 on success; nonzero on error.
 */
int
nffs_init(void)
{
    int rc;

    nffs_config_init();

    nffs_cache_clear();

    rc = nffs_stats_init();
    if (rc != 0) {
        return FS_EOS;
    }

    rc = os_mutex_init(&nffs_mutex);
    if (rc != 0) {
        return FS_EOS;
    }

    free(nffs_file_mem);
    nffs_file_mem = malloc(
        OS_MEMPOOL_BYTES(nffs_config.nc_num_files, sizeof (struct nffs_file)));
    if (nffs_file_mem == NULL) {
        return FS_ENOMEM;
    }

    free(nffs_inode_mem);
    nffs_inode_mem = malloc(
        OS_MEMPOOL_BYTES(nffs_config.nc_num_inodes,
                        sizeof (struct nffs_inode_entry)));
    if (nffs_inode_mem == NULL) {
        return FS_ENOMEM;
    }

    free(nffs_block_entry_mem);
    nffs_block_entry_mem = malloc(
        OS_MEMPOOL_BYTES(nffs_config.nc_num_blocks,
                         sizeof (struct nffs_hash_entry)));
    if (nffs_block_entry_mem == NULL) {
        return FS_ENOMEM;
    }

    free(nffs_cache_inode_mem);
    nffs_cache_inode_mem = malloc(
        OS_MEMPOOL_BYTES(nffs_config.nc_num_cache_inodes,
                         sizeof (struct nffs_cache_inode)));
    if (nffs_cache_inode_mem == NULL) {
        return FS_ENOMEM;
    }

    free(nffs_cache_block_mem);
    nffs_cache_block_mem = malloc(
        OS_MEMPOOL_BYTES(nffs_config.nc_num_cache_blocks,
                         sizeof (struct nffs_cache_block)));
    if (nffs_cache_block_mem == NULL) {
        return FS_ENOMEM;
    }

    free(nffs_dir_mem);
    nffs_dir_mem = malloc(
        OS_MEMPOOL_BYTES(nffs_config.nc_num_dirs,
                         sizeof (struct nffs_dir)));
    if (nffs_dir_mem == NULL) {
        return FS_ENOMEM;
    }

    rc = nffs_misc_reset();
    if (rc != 0) {
        return rc;
    }

    fs_register(&nffs_ops);
    return 0;
}

void
nffs_pkg_init(void)
{
    struct nffs_area_desc descs[MYNEWT_VAL(NFFS_NUM_AREAS) + 1];
    int cnt;
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    /* Initialize nffs's internal state. */
    rc = nffs_init();
    SYSINIT_PANIC_ASSERT(rc == 0);

    /* Convert the set of flash blocks we intend to use for nffs into an array
     * of nffs area descriptors.
     */
    cnt = MYNEWT_VAL(NFFS_NUM_AREAS);
    rc = nffs_misc_desc_from_flash_area(
        MYNEWT_VAL(NFFS_FLASH_AREA), &cnt, descs);
    SYSINIT_PANIC_ASSERT(rc == 0);

    /* Attempt to restore an existing nffs file system from flash. */
    rc = nffs_detect(descs);
    switch (rc) {
    case 0:
        break;

    case FS_ECORRUPT:
        /* No valid nffs instance detected; act based on configued detection
         * failure policy.
         */
        switch (MYNEWT_VAL(NFFS_DETECT_FAIL)) {
        case NFFS_DETECT_FAIL_IGNORE:
            break;

        case NFFS_DETECT_FAIL_FORMAT:
            rc = nffs_format(descs);
            SYSINIT_PANIC_ASSERT(rc == 0);
            break;

        default:
            SYSINIT_PANIC();
            break;
        }
        break;

    default:
        SYSINIT_PANIC();
        break;
    }
}
