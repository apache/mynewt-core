#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "hal/hal_flash.h"
#include "os/os_mempool.h"
#include "os/os_mutex.h"
#include "ffs_priv.h"
#include "ffs/ffs.h"

#define FFS_NUM_FILES           8
#define FFS_NUM_INODES          100
#define FFS_NUM_BLOCKS          100

static struct ffs_area ffs_area_array[FFS_MAX_AREAS];
struct ffs_area *ffs_areas = ffs_area_array;
int ffs_num_areas;
uint16_t ffs_scratch_area_id;

struct os_mempool ffs_file_pool;
struct os_mempool ffs_inode_pool;
struct os_mempool ffs_block_pool;

struct ffs_inode *ffs_root_dir;
uint32_t ffs_next_id;

static struct os_mutex ffs_mutex;

static void
ffs_lock(void)
{
    int rc;

    rc = 0;//os_mutex_pend(&ffs_mutex, 0xffffffff);
    assert(rc == 0);
}

static void
ffs_unlock(void)
{
    int rc;

    rc = 0;//os_mutex_release(&ffs_mutex);
    assert(rc == 0);
}

/**
 * Closes the specified file and invalidates the file handle.  If the file has
 * already been unlinked, and this is the last open handle to the file, this
 * operation causes the file to be deleted.
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
 *
 * @return                  The length of the file, in bytes.
 */
uint32_t
ffs_file_len(const struct ffs_file *file)
{
    uint32_t len;

    ffs_lock();
    len = file->ff_inode->fi_data_len;
    ffs_unlock();

    return len;
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
 * @param out_file          On success, a pointer to the newly-created file
 *                          handle gets written here.
 * @param path              The path of the file to open.
 * @param access_flags      Flags controlling file access; see above table.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
ffs_open(struct ffs_file **out_file, const char *path, uint8_t access_flags)
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
 * Performs a rename and / or move of the specified source path to the
 * specified * destination.  The source path can refer to either a file or a
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
 * Reads data from the specified file.  If more data is requested than remains
 * in the file, all available data is retrieved.  Note: this type of short read
 * results in a success return code.
 *
 * @param file              The file to read from.
 * @param data              The destination buffer to read into.
 * @param len               (in/out) in:  The number of bytes to read.
 *                                   out: The number of bytes actually read.
 *
 * @return                  0 on success;
 *                          nonzero on failure.
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

    rc = ffs_inode_read(file->ff_inode, file->ff_offset, data, len);
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
 * Writes the supplied data to the specified file handle.
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
 * a new one can be created via a call to ffs_format().
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

    static os_membuf_t file_buf[
        OS_MEMPOOL_SIZE(FFS_NUM_FILES, sizeof (struct ffs_file))];
    static os_membuf_t inode_buf[
        OS_MEMPOOL_SIZE(FFS_NUM_INODES, sizeof (struct ffs_inode))];
    static os_membuf_t block_buf[
        OS_MEMPOOL_SIZE(FFS_NUM_BLOCKS, sizeof (struct ffs_block))];

    rc = os_mutex_create(&ffs_mutex);
    if (rc != 0) {
        return FFS_EOS;
    }

    rc = os_mempool_init(&ffs_file_pool, FFS_NUM_FILES,
                         sizeof (struct ffs_file), &file_buf[0],
                         "ffs_file_pool");
    if (rc != 0) {
        return FFS_EOS;
    }

    rc = os_mempool_init(&ffs_inode_pool, FFS_NUM_INODES,
                         sizeof (struct ffs_inode), &inode_buf[0],
                         "ffs_inode_pool");
    if (rc != 0) {
        return FFS_EOS;
    }

    rc = os_mempool_init(&ffs_block_pool, FFS_NUM_BLOCKS,
                         sizeof (struct ffs_block), &block_buf[0],
                         "ffs_block_pool");
    if (rc != 0) {
        return FFS_EOS;
    }

    ffs_hash_init();

    return 0;
}

