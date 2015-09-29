/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "hal/hal_flash.h"
#include "os/os_mempool.h"
#include "os/os_mutex.h"
#include "os/os_malloc.h"
#include "nffs_priv.h"
#include "nffs/nffs.h"

extern struct os_task *g_current_task;  /* XXX */

struct nffs_area *nffs_areas;
uint8_t nffs_num_areas;
uint8_t nffs_scratch_area_idx;
uint16_t nffs_block_max_data_sz;

struct os_mempool nffs_file_pool;
struct os_mempool nffs_inode_entry_pool;
struct os_mempool nffs_block_entry_pool;
struct os_mempool nffs_cache_inode_pool;
struct os_mempool nffs_cache_block_pool;

void *nffs_file_mem;
void *nffs_inode_mem;
void *nffs_block_entry_mem;
void *nffs_cache_inode_mem;
void *nffs_cache_block_mem;

struct nffs_inode_entry *nffs_root_dir;
struct nffs_inode_entry *nffs_lost_found_dir;

static struct os_mutex nffs_mutex;

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

/**
 * Opens a file at the specified path.  The result of opening a nonexistent
 * file depends on the access flags specified.  All intermediate directories
 * must already exist.
 *
 * The mode strings passed to fopen() map to nffs_open()'s access flags as
 * follows:
 *   "r"  -  NFFS_ACCESS_READ
 *   "r+" -  NFFS_ACCESS_READ | NFFS_ACCESS_WRITE
 *   "w"  -  NFFS_ACCESS_WRITE | NFFS_ACCESS_TRUNCATE
 *   "w+" -  NFFS_ACCESS_READ | NFFS_ACCESS_WRITE | NFFS_ACCESS_TRUNCATE
 *   "a"  -  NFFS_ACCESS_WRITE | NFFS_ACCESS_APPEND
 *   "a+" -  NFFS_ACCESS_READ | NFFS_ACCESS_WRITE | NFFS_ACCESS_APPEND
 *
 * @param path              The path of the file to open.
 * @param access_flags      Flags controlling file access; see above table.
 * @param out_file          On success, a pointer to the newly-created file
 *                              handle gets written here.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
nffs_open(const char *path, uint8_t access_flags, struct nffs_file **out_file)
{
    int rc;

    nffs_lock();

    if (!nffs_ready()) {
        rc = NFFS_EUNINIT;
        goto done;
    }

    rc = nffs_file_open(out_file, path, access_flags);
    if (rc != 0) {
        goto done;
    }

done:
    nffs_unlock();
    if (rc != 0) {
        *out_file = NULL;
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
int
nffs_close(struct nffs_file *file)
{
    int rc;

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
int
nffs_seek(struct nffs_file *file, uint32_t offset)
{
    int rc;

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
uint32_t
nffs_getpos(const struct nffs_file *file)
{
    uint32_t offset;

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
int
nffs_file_len(struct nffs_file *file, uint32_t *out_len)
{
    int rc;

    nffs_lock();
    rc = nffs_inode_data_len(file->nf_inode_entry, out_len);
    nffs_unlock();

    return rc;
}

/**
 * Reads data from the specified file.  If more data is requested than remains
 * in the file, all available data is retrieved.  Note: this type of short read
 * results in a success return code.
 *
 * @param file              The file to read from.
 * @param len               The number of bytes to attempt to read.
 * @param out_data          The destination buffer to read into.
 * @param out_len           On success, the number of bytes actually read gets
 *                              written here.  Pass null if you don't care.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
nffs_read(struct nffs_file *file, uint32_t len, void *out_data,
          uint32_t *out_len)
{
    int rc;

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
int
nffs_write(struct nffs_file *file, const void *data, int len)
{
    int rc;

    nffs_lock();

    if (!nffs_ready()) {
        rc = NFFS_EUNINIT;
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
int
nffs_unlink(const char *path)
{
    int rc;

    nffs_lock();

    if (!nffs_ready()) {
        rc = NFFS_EUNINIT;
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
int
nffs_rename(const char *from, const char *to)
{
    int rc;

    nffs_lock();

    if (!nffs_ready()) {
        rc = NFFS_EUNINIT;
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
 * @param path                  The directory to create.
 *
 * @return                      0 on success;
 *                              nonzero on failure.
 */
int
nffs_mkdir(const char *path)
{
    int rc;

    nffs_lock();

    if (!nffs_ready()) {
        rc = NFFS_EUNINIT;
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
 *                          NFFS_ECORRUPT if no valid file system was detected;
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
 * Indicates whether a valid filesystem has been initialized, either via
 * detection or formatting.
 *
 * @return                  1 if a file system is present; 0 otherwise.
 */
int
nffs_ready(void)
{
    return nffs_root_dir != NULL;
}

/**
 * Initializes the nffs memory and data structures.  This must be called before
 * any nffs operations are attempted.
 *
 * @return                  0 on success; nonzero on error.
 */
int
nffs_init(void)
{
    int rc;

    nffs_config_init();

    nffs_cache_clear();

    rc = os_mutex_init(&nffs_mutex);
    if (rc != 0) {
        return NFFS_EOS;
    }

    free(nffs_file_mem);
    nffs_file_mem = malloc(
        OS_MEMPOOL_BYTES(nffs_config.nc_num_files, sizeof (struct nffs_file)));
    if (nffs_file_mem == NULL) {
        return NFFS_ENOMEM;
    }

    free(nffs_inode_mem);
    nffs_inode_mem = malloc(
        OS_MEMPOOL_BYTES(nffs_config.nc_num_inodes,
                        sizeof (struct nffs_inode_entry)));
    if (nffs_inode_mem == NULL) {
        return NFFS_ENOMEM;
    }

    free(nffs_block_entry_mem);
    nffs_block_entry_mem = malloc(
        OS_MEMPOOL_BYTES(nffs_config.nc_num_blocks,
                         sizeof (struct nffs_hash_entry)));
    if (nffs_block_entry_mem == NULL) {
        return NFFS_ENOMEM;
    }

    free(nffs_cache_inode_mem);
    nffs_cache_inode_mem = malloc(
        OS_MEMPOOL_BYTES(nffs_config.nc_num_cache_inodes,
                         sizeof (struct nffs_cache_inode)));
    if (nffs_cache_inode_mem == NULL) {
        return NFFS_ENOMEM;
    }

    free(nffs_cache_block_mem);
    nffs_cache_block_mem = malloc(
        OS_MEMPOOL_BYTES(nffs_config.nc_num_cache_blocks,
                         sizeof (struct nffs_cache_block)));
    if (nffs_cache_block_mem == NULL) {
        return NFFS_ENOMEM;
    }

    rc = nffs_misc_reset();
    if (rc != 0) {
        return rc;
    }

    return 0;
}
