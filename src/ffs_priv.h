#ifndef H_FFS_PRIV_
#define H_FFS_PRIV_

#include <inttypes.h>
#include "os/queue.h"
#include "os/os_mempool.h"
#include "ffs/ffs.h"

#define FFS_DEBUG 1

#define FFS_ID_DIR_MIN              0
#define FFS_ID_DIR_MAX              0x10000000
#define FFS_ID_FILE_MIN             0x10000000
#define FFS_ID_FILE_MAX             0x80000000
#define FFS_ID_BLOCK_MIN            0x80000000
#define FFS_ID_BLOCK_MAX            0xffffffff

#define FFS_ID_ROOT_DIR             0
#define FFS_ID_NONE                 0xffffffff

#define FFS_AREA_MAGIC0             0xb98a31e2
#define FFS_AREA_MAGIC1             0x7fb0428c
#define FFS_AREA_MAGIC2             0xace08253
#define FFS_AREA_MAGIC3             0xb185fc8e
#define FFS_BLOCK_MAGIC             0x53ba23b9
#define FFS_INODE_MAGIC             0x925f8bc0

#define FFS_AREA_ID_NONE            0xff
#define FFS_AREA_VER                0
#define FFS_AREA_OFFSET_ID          23

#define FFS_SHORT_FILENAME_LEN      3

#define FFS_HASH_SIZE               256

#define FFS_BLOCK_MAX_DATA_SZ_MAX   2048

/** On-disk representation of an area header. */
struct ffs_disk_area {
    uint32_t fda_magic[4];  /* FFS_AREA_MAGIC{0,1,2,3} */
    uint32_t fda_length;    /* Total size of area, in bytes. */
    uint8_t fda_ver;        /* Current ffs version: 0 */
    uint8_t fda_gc_seq;     /* Garbage collection count. */
    uint8_t reserved8;
    uint8_t fda_id;         /* 0xff if scratch area. */
};

/** On-disk representation of an inode (file or directory). */
struct ffs_disk_inode {
    uint32_t fdi_magic;         /* FFS_INODE_MAGIC */
    uint32_t fdi_id;            /* Unique object ID. */
    uint32_t fdi_seq;           /* Sequence number; greater supersedes
                                   lesser. */
    uint32_t fdi_parent_id;     /* Object ID of parent directory inode. */
    uint8_t reserved8;
    uint8_t fdi_filename_len;   /* Length of filename, in bytes. */
    uint16_t fdi_crc16;         /* Covers rest of header and filename. */
    /* Followed by filename. */
};

#define FFS_DISK_INODE_OFFSET_CRC  18

/** On-disk representation of a data block. */
struct ffs_disk_block {
    uint32_t fdb_magic;     /* FFS_BLOCK_MAGIC */
    uint32_t fdb_id;        /* Unique object ID. */
    uint32_t fdb_seq;       /* Sequence number; greater supersedes lesser. */
    uint32_t fdb_inode_id;  /* Object ID of owning inode. */
    uint32_t fdb_prev_id;   /* Object ID of previous block in file;
                               FFS_ID_NONE if this is the first block. */
    uint16_t fdb_data_len;  /* Length of data contents, in bytes. */
    uint16_t fdb_crc16;     /* Covers rest of header and data. */
    /* Followed by 'fdb_data_len' bytes of data. */
};

#define FFS_DISK_BLOCK_OFFSET_CRC  20

/**
 * What gets stored in the hash table.  Each entry represents a data block or
 * an inode.
 */
struct ffs_hash_entry {
    SLIST_ENTRY(ffs_hash_entry) fhe_next;
    uint32_t fhe_id;        /* 0 - 0x7fffffff if inode; else if block. */
    uint32_t fhe_flash_loc; /* Upper-byte = area idx; rest = area offset. */
};


SLIST_HEAD(ffs_hash_list, ffs_hash_entry);
SLIST_HEAD(ffs_inode_list, ffs_inode_entry);

/** Each inode hash entry is actually one of these. */
struct ffs_inode_entry {
    struct ffs_hash_entry fie_hash_entry;
    SLIST_ENTRY(ffs_inode_entry) fie_sibling_next;
    union {
        struct ffs_inode_list fie_child_list;           /* If directory */
        struct ffs_hash_entry *fie_last_block_entry;    /* If file */
    };
    uint8_t fie_refcnt;
};

/** Full inode representation; not stored permanently RAM. */
struct ffs_inode {
    struct ffs_inode_entry *fi_inode_entry;
    uint32_t fi_seq;
    struct ffs_inode_entry *fi_parent; /* Pointer to parent directory inode. */
    uint8_t fi_filename_len;
    uint8_t fi_filename[FFS_SHORT_FILENAME_LEN]; /* 3 bytes. */
};

/** Full data block representation; not stored permanently RAM. */
struct ffs_block {
    struct ffs_hash_entry *fb_hash_entry;
    uint32_t fb_seq;
    struct ffs_inode_entry *fb_inode_entry;
    struct ffs_hash_entry *fb_prev;
    uint16_t fb_data_len;
    uint16_t reserved16;
};

struct ffs_file {
    struct ffs_inode_entry *ff_inode_entry;
    uint32_t ff_offset;
    uint8_t ff_access_flags;
};

struct ffs_area {
    uint32_t fa_offset;
    uint32_t fa_length;
    uint32_t fa_cur;
    uint16_t fa_id;
    uint8_t fa_gc_seq;
};

struct ffs_disk_object {
    int fdo_type;
    uint8_t fdo_area_idx;
    uint32_t fdo_offset;
    union {
        struct ffs_disk_inode fdo_disk_inode;
        struct ffs_disk_block fdo_disk_block;
    };
};

struct ffs_seek_info {
    struct ffs_block fsi_last_block;
    uint32_t fsi_block_file_off;
    uint32_t fsi_file_len;
};

#define FFS_OBJECT_TYPE_INODE   1
#define FFS_OBJECT_TYPE_BLOCK   2

#define FFS_PATH_TOKEN_NONE     0
#define FFS_PATH_TOKEN_BRANCH   1
#define FFS_PATH_TOKEN_LEAF     2

struct ffs_path_parser {
    int fpp_token_type;
    const char *fpp_path;
    const char *fpp_token;
    int fpp_token_len;
    int fpp_off;
};

/** Represents a single data block. */
struct ffs_cache_block {
    struct ffs_hash_entry *fcb_entry;            /* Pointer to real block. */
    uint32_t fcb_seq;
    uint16_t fcb_data_len;
    TAILQ_ENTRY(ffs_cache_block_entry) fcb_link; /* Next / prev block. */
    uint32_t fcb_file_offset;              /* File offset of this block. */
};

TAILQ_HEAD(ffs_cache_block_list, ffs_cache_block);

/** Represents all or part of a file. */
struct ffs_cache_inode {
    TAILQ_ENTRY(ffs_cache_inode) fci_link;  /* Sorted; LRU at tail. */
    struct ffs_inode_entry *fci_inode_entry;     /* Pointer to real file. */
    struct ffs_cache_block_list fci_block_list;  /* List of cached blocks. */
    uint32_t fci_file_size;                      /* Total file size. */
    uint32_t fci_cache_length;       /* Cumulative length of cached data. */
};

extern void *ffs_file_mem;
extern void *ffs_block_entry_mem;
extern void *ffs_inode_mem;
extern void *ffs_cache_inode_mem;
extern struct os_mempool ffs_file_pool;
extern struct os_mempool ffs_inode_entry_pool;
extern struct os_mempool ffs_block_entry_pool;
extern struct os_mempool ffs_cache_inode_pool;
extern uint32_t ffs_hash_next_file_id;
extern uint32_t ffs_hash_next_dir_id;
extern uint32_t ffs_hash_next_block_id;
extern struct ffs_area *ffs_areas;
extern uint8_t ffs_num_areas;
extern uint8_t ffs_scratch_area_idx;
extern uint16_t ffs_block_max_data_sz;

#define FFS_FLASH_BUF_SZ        256
extern uint8_t ffs_flash_buf[FFS_FLASH_BUF_SZ];

extern struct ffs_hash_list ffs_hash[FFS_HASH_SIZE];
extern struct ffs_inode_entry *ffs_root_dir;

/* @area */
int ffs_area_magic_is_set(const struct ffs_disk_area *disk_area);
int ffs_area_is_scratch(const struct ffs_disk_area *disk_area);
void ffs_area_to_disk(const struct ffs_area *area,
                      struct ffs_disk_area *out_disk_area);
uint32_t ffs_area_free_space(const struct ffs_area *area);
int ffs_area_find_corrupt_scratch(uint16_t *out_good_idx,
                                  uint16_t *out_bad_idx);

/* @block */
struct ffs_hash_entry *ffs_block_entry_alloc(void);
void ffs_block_entry_free(struct ffs_hash_entry *entry);
int ffs_block_read_disk(uint8_t area_idx, uint32_t area_offset,
                        struct ffs_disk_block *out_disk_block);
int ffs_block_write_disk(const struct ffs_disk_block *disk_block,
                         const void *data,
                         uint8_t *out_area_idx, uint32_t *out_area_offset);
int ffs_block_delete_from_ram(struct ffs_hash_entry *entry);
void ffs_block_delete_list_from_ram(struct ffs_block *first,
                                    struct ffs_block *last);
void ffs_block_delete_list_from_disk(const struct ffs_block *first,
                                     const struct ffs_block *last);
void ffs_block_to_disk(const struct ffs_block *block,
                       struct ffs_disk_block *out_disk_block);
int ffs_block_from_hash_entry_no_ptrs(struct ffs_block *out_block,
                                      struct ffs_hash_entry *entry);
int ffs_block_from_hash_entry(struct ffs_block *out_block,
                              struct ffs_hash_entry *entry);

/* @cache */
void ffs_cache_inode_delete(const struct ffs_inode_entry *inode_entry);
int ffs_cache_inode_assure(struct ffs_cache_inode **out_entry,
                           struct ffs_inode_entry *inode_entry);
void ffs_cache_clear(void);

/* @crc */
int ffs_crc_flash(uint16_t initial_crc, uint8_t area_idx, uint32_t area_offset,
                  uint32_t len, uint16_t *out_crc);
uint16_t ffs_crc_disk_block_hdr(const struct ffs_disk_block *disk_block);
int ffs_crc_disk_block_validate(const struct ffs_disk_block *disk_block,
                                uint8_t area_idx, uint32_t area_offset);
void ffs_crc_disk_block_fill(struct ffs_disk_block *disk_block,
                             const void *data);
int ffs_crc_disk_inode_validate(const struct ffs_disk_inode *disk_inode,
                                uint8_t area_idx, uint32_t area_offset);
void ffs_crc_disk_inode_fill(struct ffs_disk_inode *disk_inode,
                             const char *filename);

/* @config */
void ffs_config_init(void);

/* @file */
int ffs_file_open(struct ffs_file **out_file, const char *filename,
                  uint8_t access_flags);
int ffs_file_seek(struct ffs_file *file, uint32_t offset);
int ffs_file_close(struct ffs_file *file);
int ffs_file_new(struct ffs_inode_entry *parent, const char *filename,
                 uint8_t filename_len, int is_dir,
                 struct ffs_inode_entry **out_inode_entry);

/* @format */
int ffs_format_area(uint8_t area_idx, int is_scratch);
int ffs_format_from_scratch_area(uint8_t area_idx, uint8_t area_id);
int ffs_format_full(const struct ffs_area_desc *area_descs);

/* @gc */
int ffs_gc(uint8_t *out_area_idx);
int ffs_gc_until(uint32_t space, uint8_t *out_area_idx);

/* @flash */
struct ffs_area *ffs_flash_find_area(uint16_t logical_id);
int ffs_flash_read(uint8_t area_idx, uint32_t offset,
                   void *data, uint32_t len);
int ffs_flash_write(uint8_t area_idx, uint32_t offset,
                    const void *data, uint32_t len);
int ffs_flash_copy(uint8_t area_id_from, uint32_t offset_from,
                   uint8_t area_id_to, uint32_t offset_to,
                   uint32_t len);
uint32_t ffs_flash_loc(uint8_t area_idx, uint32_t offset);
void ffs_flash_loc_expand(uint32_t flash_loc, uint8_t *out_area_idx,
                          uint32_t *out_area_offset);

/* @hash */
int ffs_hash_id_is_dir(uint32_t id);
int ffs_hash_id_is_file(uint32_t id);
int ffs_hash_id_is_inode(uint32_t id);
int ffs_hash_id_is_block(uint32_t id);
struct ffs_hash_entry *ffs_hash_find(uint32_t id);
struct ffs_inode_entry *ffs_hash_find_inode(uint32_t id);
struct ffs_hash_entry *ffs_hash_find_block(uint32_t id);
void ffs_hash_insert(struct ffs_hash_entry *entry);
void ffs_hash_remove(struct ffs_hash_entry *entry);
void ffs_hash_init(void);

/* @inode */
struct ffs_inode_entry *ffs_inode_entry_alloc(void);
void ffs_inode_entry_free(struct ffs_inode_entry *inode_entry);
int ffs_inode_calc_data_length(struct ffs_inode_entry *inode_entry,
                               uint32_t *out_len);
int ffs_inode_data_len(struct ffs_inode_entry *inode_entry, uint32_t *out_len);
uint32_t ffs_inode_parent_id(const struct ffs_inode *inode);
int ffs_inode_delete_from_disk(struct ffs_inode *inode);
int ffs_inode_entry_from_disk(struct ffs_inode_entry *out_inode,
                              const struct ffs_disk_inode *disk_inode,
                              uint8_t area_idx, uint32_t offset);
int ffs_inode_rename(struct ffs_inode_entry *inode_entry,
                     struct ffs_inode_entry *new_parent, const char *filename);
void ffs_inode_insert_block(struct ffs_inode *inode, struct ffs_block *block);
int ffs_inode_read_disk(uint8_t area_idx, uint32_t offset,
                        struct ffs_disk_inode *out_disk_inode);
int ffs_inode_write_disk(const struct ffs_disk_inode *disk_inode,
                         const char *filename, uint8_t area_idx,
                         uint32_t offset);
int ffs_inode_dec_refcnt(struct ffs_inode_entry *inode_entry);
int ffs_inode_add_child(struct ffs_inode_entry *parent,
                        struct ffs_inode_entry *child);
void ffs_inode_remove_child(struct ffs_inode *child);
int ffs_inode_is_root(const struct ffs_disk_inode *disk_inode);
int ffs_inode_filename_cmp_ram(const struct ffs_inode *inode,
                               const char *name, int name_len,
                               int *result);
int ffs_inode_filename_cmp_flash(const struct ffs_inode *inode1,
                                 const struct ffs_inode *inode2,
                                 int *result);
int ffs_inode_read(struct ffs_inode_entry *inode_entry, uint32_t offset,
                   uint32_t len, void *data, uint32_t *out_len);
int ffs_inode_seek(struct ffs_inode_entry *inode_entry, uint32_t offset,
                   uint32_t length, struct ffs_seek_info *out_seek_info);
int ffs_inode_from_entry(struct ffs_inode *out_inode,
                         struct ffs_inode_entry *entry);
int ffs_inode_unlink_from_ram(struct ffs_inode *inode,
                              struct ffs_hash_entry **out_next);
int ffs_inode_unlink(struct ffs_inode *inode);

/* @misc */
int ffs_misc_reserve_space(uint16_t space,
                           uint8_t *out_area_idx, uint32_t *out_area_offset);
int ffs_misc_set_num_areas(uint8_t num_areas);
int ffs_misc_validate_root_dir(void);
int ffs_misc_validate_scratch(void);
int ffs_misc_reset(void);
int ffs_misc_set_max_block_data_len(uint16_t min_data_len);

/* @path */
int ffs_path_parse_next(struct ffs_path_parser *parser);
void ffs_path_parser_new(struct ffs_path_parser *parser, const char *path);
int ffs_path_find(struct ffs_path_parser *parser,
                  struct ffs_inode_entry **out_inode_entry,
                  struct ffs_inode_entry **out_parent);
int ffs_path_find_inode_entry(const char *filename,
                              struct ffs_inode_entry **out_inode_entry);
int ffs_path_unlink(const char *filename);
int ffs_path_rename(const char *from, const char *to);
int ffs_path_new_dir(const char *path);

/* @restore */
int ffs_restore_full(const struct ffs_area_desc *area_descs);

/* @write */
int ffs_write_to_file(struct ffs_file *file, const void *data, int len);


#define FFS_HASH_FOREACH(entry, i)                                      \
    for ((i) = 0; (i) < FFS_HASH_SIZE; (i)++)                           \
        SLIST_FOREACH((entry), &ffs_hash[i], fhe_next)

#define FFS_FLASH_LOC_NONE  ffs_flash_loc(FFS_AREA_ID_NONE, 0)

#endif

