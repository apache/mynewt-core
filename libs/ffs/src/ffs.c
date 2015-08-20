#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "hal/hal_flash.h"
#include "os/os_mempool.h"
#include "os/os_mutex.h"
#include "os/os_malloc.h"
#include "ffs_priv.h"
#include "ffs/ffs.h"

extern struct os_task *g_current_task;  /* XXX */

struct ffs_area *ffs_areas;
uint8_t ffs_num_areas;
uint8_t ffs_scratch_area_idx;
uint16_t ffs_block_max_data_sz;

struct os_mempool ffs_file_pool;
struct os_mempool ffs_inode_entry_pool;
struct os_mempool ffs_block_entry_pool;
struct os_mempool ffs_cache_inode_pool;
struct os_mempool ffs_cache_block_pool;

void *ffs_file_mem;
void *ffs_inode_mem;
void *ffs_block_entry_mem;
void *ffs_cache_inode_mem;
void *ffs_cache_block_mem;

struct ffs_inode_entry *ffs_root_dir;

static struct os_mutex ffs_mutex;

static void
ffs_lock(void)
{
    int rc;

    if (g_current_task != NULL) { /* XXX */
        rc = os_mutex_pend(&ffs_mutex, 0xffffffff);
        assert(rc == 0);
    }
}

static void
ffs_unlock(void)
{
    int rc;

    if (g_current_task != NULL) { /* XXX */
        rc = os_mutex_release(&ffs_mutex);
        assert(rc == 0);
    }
}

/**
 * Opens a file at the specified path.  The result of opening a nonexistent
 * file depends on the access flags specified.  All intermediate directories
 * must have already been created.
 *
 * The mode strings passed to fopen() map to ffs_open()'s access flags as
 * follows:
 *   "r"  -  FFS_ACCESS_READ
 *   "r+" -  FFS_ACCESS_READ | FFS_ACCESS_WRITE
 *   "w"  -  FFS_ACCESS_WRITE | FFS_ACCESS_TRUNCATE
 *   "w+" -  FFS_ACCESS_READ | FFS_ACCESS_WRITE | FFS_ACCESS_TRUNCATE
 *   "a"  -  FFS_ACCESS_WRITE | FFS_ACCESS_APPEND
 *   "a+" -  FFS_ACCESS_READ | FFS_ACCESS_WRITE | FFS_ACCESS_APPEND
 *
 * @param path              The path of the file to open.
 * @param access_flags      Flags controlling file access; see above table.
 * @param out_file          On success, a pointer to the newly-created file
 *                              handle gets written here.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
ffs_open(const char *path, uint8_t access_flags, struct ffs_file **out_file)
{
    int rc;

    ffs_lock();

    if (!ffs_ready()) {
        rc = FFS_EUNINIT;
        goto done;
    }

    rc = ffs_file_open(out_file, path, access_flags);
    if (rc != 0) {
        goto done;
    }

done:
    ffs_unlock();
    if (rc != 0) {
        *out_file = NULL;
    }
    return rc;
}

/**
 * Closes the specified file and invalidates the file handle.  If the file has
 * already been unlinked, and this is the last open handle to the file, this
 * operation causes the file to be deleted.
 *
 * @param file              The file handle to close.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
ffs_close(struct ffs_file *file)
{
    int rc;

    if (file == NULL) {
        return 0;
    }

    ffs_lock();
    rc = ffs_file_close(file);
    ffs_unlock();

    return rc;
}

/**
 * Positions a file's read and write pointer at the specified offset.  The
 * offset is expressed as the number of bytes from the start of the file (i.e.,
 * seeking to 0 places the pointer at the first byte in the file).
 *
 * @param file              The file to reposition.
 * @param offset            The offset from the start of the file to seek to.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
ffs_seek(struct ffs_file *file, uint32_t offset)
{
    int rc;

    ffs_lock();
    rc = ffs_file_seek(file, offset);
    ffs_unlock();

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
ffs_getpos(const struct ffs_file *file)
{
    uint32_t offset;

    ffs_lock();
    offset = file->ff_offset;
    ffs_unlock();

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
ffs_file_len(struct ffs_file *file, uint32_t *out_len)
{
    int rc;

    ffs_lock();
    rc = ffs_inode_data_len(file->ff_inode_entry, out_len);
    ffs_unlock();

    return rc;
}

/**
 * Reads data from the specified file.  If more data is requested than remains
 * in the file, all available data is retrieved.  Note: this type of short read
 * results in a success return code.
 *
 * @param file              The file to read from.
 * @param data              The destination buffer to read into.
 * @param len               (in/out) in:  The number of bytes to read.
 *                                   out: The number of bytes actually read.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
ffs_read(struct ffs_file *file, void *data, uint32_t *len)
{
    int rc;

    ffs_lock();

    if (!ffs_ready()) {
        rc = FFS_EUNINIT;
        goto done;
    }

    rc = ffs_inode_read(file->ff_inode_entry, file->ff_offset, *len, data, len);
    if (rc != 0) {
        goto done;
    }

    file->ff_offset += *len;

    rc = 0;

done:
    ffs_unlock();
    return rc;
}

/**
 * Writes the supplied data to the current offset of the specified file handle.
 *
 * @param file              The file to write to.
 * @param data              The data to write.
 * @param len               The number of bytes to write.
 *
 * @return                  0 on success;
 *                          nonzero on failure.
 */
int
ffs_write(struct ffs_file *file, const void *data, int len)
{
    int rc;

    ffs_lock();

    if (!ffs_ready()) {
        rc = FFS_EUNINIT;
        goto done;
    }

    rc = ffs_write_to_file(file, data, len);
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    ffs_unlock();
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
ffs_unlink(const char *path)
{
    int rc;

    ffs_lock();

    if (!ffs_ready()) {
        rc = FFS_EUNINIT;
        goto done;
    }

    rc = ffs_path_unlink(path);
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    ffs_unlock();
    return rc;
}

/**
 * Performs a rename and / or move of the specified source path to the
 * specified destination.  The source path can refer to either a file or a
 * directory.  All intermediate directories in the destination path must
 * already have been created.  If the source path refers to a file, the
 * destination path must contain a full filename path (i.e., if performing a
 * move, the destination path should end with the same filename in the source
 * path).  If an object already exists at the specified destination path, this
 * function causes it to be unlinked prior to the rename (i.e., the destination
 * gets clobbered).
 *
 * @param from              The source path.
 * @param to                The destination path.
 *
 * @return                  0 on success;
 *                          nonzero on failure.
 */
int
ffs_rename(const char *from, const char *to)
{
    int rc;

    ffs_lock();

    if (!ffs_ready()) {
        rc = FFS_EUNINIT;
        goto done;
    }

    rc = ffs_path_rename(from, to);
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    ffs_unlock();
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
ffs_mkdir(const char *path)
{
    int rc;

    ffs_lock();

    if (!ffs_ready()) {
        rc = FFS_EUNINIT;
        goto done;
    }

    rc = ffs_path_new_dir(path);
    if (rc != 0) {
        goto done;
    }

done:
    ffs_unlock();
    return rc;
}

/**
 * Erases all the specified areas and initializes them with a clean ffs
 * file system.
 *
 * @param area_descs        The set of areas to format.
 *
 * @return                  0 on success;
 *                          nonzero on failure.
 */
int
ffs_format(const struct ffs_area_desc *area_descs)
{
    int rc;

    ffs_lock();
    rc = ffs_format_full(area_descs);
    ffs_unlock();

    return rc;
}

/**
 * Searches for a valid ffs file system among the specified areas.  This
 * function succeeds if a file system is detected among any subset of the
 * supplied areas.  If the area set does not contain a valid file system,
 * a new one can be created via a separate call to ffs_format().
 *
 * @param area_descs        The area set to search.  This array must be
 *                              terminated with a 0-length area.
 *
 * @return                  0 on success;
 *                          FFS_ECORRUPT if no valid file system was detected;
 *                          other nonzero on error.
 */
int
ffs_detect(const struct ffs_area_desc *area_descs)
{
    int rc;

    ffs_lock();
    rc = ffs_restore_full(area_descs);
    ffs_unlock();

    return rc;
}

/**
 * Indicates whether a valid filesystem has been initialized, either via
 * detection or formatting.
 *
 * @return                  1 if a file system is present; 0 otherwise.
 */
int
ffs_ready(void)
{
    return ffs_root_dir != NULL;
}

/**
 * Initializes the ffs memory and data structures.  This must be called before
 * any ffs operations are attempted.
 *
 * @return                  0 on success; nonzero on error.
 */
int
ffs_init(void)
{
    int rc;

    ffs_config_init();

    ffs_cache_clear();

    rc = os_mutex_create(&ffs_mutex);
    if (rc != 0) {
        return FFS_EOS;
    }

    free(ffs_file_mem);
    ffs_file_mem = malloc(
        OS_MEMPOOL_BYTES(ffs_config.fc_num_files, sizeof (struct ffs_file)));
    if (ffs_file_mem == NULL) {
        return FFS_ENOMEM;
    }

    free(ffs_inode_mem);
    ffs_inode_mem = malloc(
        OS_MEMPOOL_BYTES(ffs_config.fc_num_inodes,
                        sizeof (struct ffs_inode_entry)));
    if (ffs_inode_mem == NULL) {
        return FFS_ENOMEM;
    }

    free(ffs_block_entry_mem);
    ffs_block_entry_mem = malloc(
        OS_MEMPOOL_BYTES(ffs_config.fc_num_blocks,
                         sizeof (struct ffs_hash_entry)));
    if (ffs_block_entry_mem == NULL) {
        return FFS_ENOMEM;
    }

    free(ffs_cache_inode_mem);
    ffs_cache_inode_mem = malloc(
        OS_MEMPOOL_BYTES(ffs_config.fc_num_cache_inodes,
                         sizeof (struct ffs_cache_inode)));
    if (ffs_cache_inode_mem == NULL) {
        return FFS_ENOMEM;
    }

    free(ffs_cache_block_mem);
    ffs_cache_block_mem = malloc(
        OS_MEMPOOL_BYTES(ffs_config.fc_num_cache_blocks,
                         sizeof (struct ffs_cache_block)));
    if (ffs_cache_block_mem == NULL) {
        return FFS_ENOMEM;
    }

    rc = ffs_misc_reset();
    if (rc != 0) {
        return rc;
    }

    return 0;
}
