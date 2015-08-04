#ifndef H_FFS_PRIV_
#define H_FFS_PRIV_

#include <inttypes.h>
#include "os/queue.h"
#include "os/os_mempool.h"
#include "ffs/ffs.h"

#define FFS_ID_NONE                 0xffffffff

#define FFS_AREA_MAGIC0             0xb98a31e2
#define FFS_AREA_MAGIC1             0x7fb0428c
#define FFS_AREA_MAGIC2             0xace08253
#define FFS_AREA_MAGIC3             0xb185fc8e
#define FFS_BLOCK_MAGIC             0x53ba23b9
#define FFS_INODE_MAGIC             0x925f8bc0

#define FFS_AREA_ID_NONE            0xffff
#define FFS_AREA_VER                0
#define FFS_AREA_OFFSET_ID          22

#define FFS_SHORT_FILENAME_LEN      1

#define FFS_HASH_SIZE               256

/* These inode flags are used in the disk and RAM representations. */
#define FFS_INODE_F_DELETED         0x01
#define FFS_INODE_F_DIRECTORY       0x02

/* These inode flags are only used in the RAM representation. */
#define FFS_INODE_F_DUMMY           0x04
#define FFS_INODE_F_TEST            0x80

/* These block flags are used in the disk and RAM representations. */
#define FFS_BLOCK_F_DELETED         0x01

/* These block flags are only used in the RAM representation. */
#define FFS_BLOCK_F_DUMMY           0x02

#define FFS_BLOCK_MAX_DATA_SZ_MAX   2048

/** On-disk representation of an area header. */
struct ffs_disk_area {
    uint32_t fda_magic[4];  /* FFS_AREA_MAGIC{0,1,2,3} */
    uint32_t fda_length;    /* Total size of area, in bytes. */
    uint8_t fda_ver;        /* Current ffs version: 0 */
    uint8_t fda_gc_seq;     /* Garbage collection count. */
    uint16_t fda_id;        /* 0xffff if scratch area. */
    /* XXX: ECC for area header. */
};

/** On-disk representation of an inode (file or directory). */
struct ffs_disk_inode {
    uint32_t fdi_magic;     /* FFS_INODE_MAGIC */
    uint32_t fdi_id;        /* Unique object ID. */
    uint32_t fdi_seq;       /* Sequence number; greater supersedes lesser. */
    uint32_t fdi_parent_id; /* Object ID of parent directory inode. */
    uint16_t fdi_flags;     /* FFS_INODE_F_[...] */
    uint8_t reserved8;
    uint8_t fdi_filename_len;   /* Length of filename, in bytes. */
    /* XXX: ECC for inode header and filename. */
    /* Followed by filename. */
};

/** On-disk representation of a data block. */
struct ffs_disk_block {
    uint32_t fdb_magic;     /* FFS_BLOCK_MAGIC */
    uint32_t fdb_id;        /* Unique object ID. */
    uint32_t fdb_seq;       /* Sequence number; greater supersedes lesser. */
    uint32_t fdb_rank;      /* Relative offset within file; 0 = first. */
    uint32_t fdb_inode_id;  /* Object ID of owning inode. */
    uint16_t fdb_flags;     /* FFS_BLOCK_F_[...] */
    uint16_t fdb_data_len;  /* Length of data contents, in bytes. */
    /* XXX: ECC for block header and contents. */
    /* Followed by 'length' bytes of data. */
};

#define FFS_OBJECT_TYPE_INODE   1
#define FFS_OBJECT_TYPE_BLOCK   2

struct ffs_object {
    SLIST_ENTRY(ffs_object) fb_hash_next;
    uint32_t fo_id;
    uint32_t fo_seq;
    uint32_t fo_area_offset;
    uint16_t fo_area_idx;
    uint8_t fo_type;
};

struct ffs_block {
    struct ffs_object fb_object;
    struct ffs_inode *fb_inode;
    SLIST_ENTRY(ffs_block) fb_next;
    uint32_t fb_rank;
    uint16_t fb_data_len;
    uint8_t fb_flags;
};

SLIST_HEAD(ffs_block_list, ffs_block);
SLIST_HEAD(ffs_inode_list, ffs_inode);

struct ffs_inode {
    struct ffs_object fi_object;
    SLIST_ENTRY(ffs_inode) fi_sibling_next;
    union {
        struct ffs_block_list fi_block_list;    /* If file. */
        struct ffs_inode_list fi_child_list;    /* If directory. */
    };
    struct ffs_inode *fi_parent;    /* Pointer to parent directory inode. */
    uint8_t fi_filename_len;
    uint8_t fi_flags;
    uint8_t fi_refcnt;
    uint8_t fi_filename[FFS_SHORT_FILENAME_LEN];
};

struct ffs_file {
    struct ffs_inode *ff_inode;
    uint32_t ff_offset;
    uint8_t ff_access_flags;
};

struct ffs_area {
    uint32_t fa_offset;
    uint32_t fa_length;
    uint32_t fa_cur;
    uint8_t fa_gc_seq;
    uint16_t fa_id;
};

struct ffs_disk_object {
    int fdo_type;
    uint16_t fdo_area_idx;
    uint32_t fdo_offset;
    union {
        struct ffs_disk_inode fdo_disk_inode;
        struct ffs_disk_block fdo_disk_block;
    };
};

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

extern void *ffs_file_mem;
extern void *ffs_block_mem;
extern void *ffs_inode_mem;
extern struct os_mempool ffs_file_pool;
extern struct os_mempool ffs_inode_pool;
extern struct os_mempool ffs_block_pool;
extern uint32_t ffs_next_id;
extern struct ffs_area *ffs_areas;
extern uint16_t ffs_num_areas;
extern uint16_t ffs_scratch_area_idx;
extern uint16_t ffs_block_max_data_sz;

SLIST_HEAD(ffs_object_list, ffs_object);
extern struct ffs_object_list ffs_hash[FFS_HASH_SIZE];
extern struct ffs_inode *ffs_root_dir;

struct ffs_area *ffs_flash_find_area(uint16_t logical_id);
int ffs_flash_read(uint16_t area_idx, uint32_t offset,
                   void *data, uint32_t len);
int ffs_flash_write(uint16_t area_idx, uint32_t offset,
                    const void *data, uint32_t len);
int ffs_flash_copy(uint16_t area_id_from, uint32_t offset_from,
                   uint16_t area_id_to, uint32_t offset_to,
                   uint32_t len);

void ffs_config_init(void);

struct ffs_object *ffs_hash_find(uint32_t id);
int ffs_hash_find_inode(struct ffs_inode **out_inode, uint32_t id);
int ffs_hash_find_block(struct ffs_block **out_block, uint32_t id);
void ffs_hash_insert(struct ffs_object *object);
void ffs_hash_remove(struct ffs_object *object);
void ffs_hash_init(void);

int ffs_path_parse_next(struct ffs_path_parser *parser);
void ffs_path_parser_new(struct ffs_path_parser *parser, const char *path);
int ffs_path_find(struct ffs_path_parser *parser,
                  struct ffs_inode **out_inode,
                  struct ffs_inode **out_parent);
int ffs_path_find_inode(struct ffs_inode **out_inode, const char *filename);
int ffs_path_unlink(const char *filename);
int ffs_path_rename(const char *from, const char *to);
int ffs_path_new_dir(const char *path);

int ffs_restore_full(const struct ffs_area_desc *area_descs);

struct ffs_inode *ffs_inode_alloc(void);
void ffs_inode_free(struct ffs_inode *inode);
uint32_t ffs_inode_calc_data_length(const struct ffs_inode *inode);
uint32_t ffs_inode_parent_id(const struct ffs_inode *inode);
void ffs_inode_delete_from_ram(struct ffs_inode *inode);
int ffs_inode_delete_from_disk(const struct ffs_inode *inode);
int ffs_inode_from_disk(struct ffs_inode *out_inode,
                        const struct ffs_disk_inode *disk_inode,
                        uint16_t area_idx, uint32_t offset);
int ffs_inode_rename(struct ffs_inode *inode, const char *filename);
void ffs_inode_insert_block(struct ffs_inode *inode, struct ffs_block *block);
int ffs_inode_read_disk(struct ffs_disk_inode *out_disk_inode,
                        char *out_filename, uint16_t area_idx,
                        uint32_t offset);
int ffs_inode_write_disk(const struct ffs_disk_inode *disk_inode,
                         const char *filename, uint16_t area_idx,
                         uint32_t offset);
int ffs_inode_seek(const struct ffs_inode *inode, uint32_t offset,
                   struct ffs_block **out_prev_block,
                   struct ffs_block **out_block, uint32_t *out_block_off);
void ffs_inode_dec_refcnt(struct ffs_inode *inode);
int ffs_inode_add_child(struct ffs_inode *parent, struct ffs_inode *child);
void ffs_inode_remove_child(struct ffs_inode *child);
void ffs_inode_delete_from_ram(struct ffs_inode *inode);
int ffs_inode_is_root(const struct ffs_disk_inode *disk_inode);
int ffs_inode_filename_cmp_ram(int *result, const struct ffs_inode *inode,
                               const char *name, int name_len);
int ffs_inode_filename_cmp_flash(int *result, const struct ffs_inode *inode1,
                                 const struct ffs_inode *inode2);
int ffs_inode_read(const struct ffs_inode *inode, uint32_t offset,
                   void *data, uint32_t *len);

struct ffs_block *ffs_block_alloc(void);
void ffs_block_free(struct ffs_block *block);
uint32_t ffs_block_disk_size(const struct ffs_block *block);
int ffs_block_read_disk(struct ffs_disk_block *out_disk_block,
                        uint16_t area_idx, uint32_t offset);
int ffs_block_write_disk(uint16_t *out_area_idx, uint32_t *out_offset,
                         const struct ffs_disk_block *disk_block,
                         const void *data);
void ffs_block_delete_from_ram(struct ffs_block *block);
void ffs_block_delete_list_from_ram(struct ffs_block *first,
                                    struct ffs_block *last);
int ffs_block_delete_from_disk(const struct ffs_block *block);
void ffs_block_delete_list_from_disk(const struct ffs_block *first,
                                     const struct ffs_block *last);
void ffs_block_from_disk(struct ffs_block *out_block,
                         const struct ffs_disk_block *disk_block,
                         uint16_t area_idx, uint32_t offset);

int ffs_misc_reserve_space(uint16_t *out_area_idx, uint32_t *out_offset,
                           uint16_t size);
int ffs_misc_set_num_areas(uint16_t num_areas);

int ffs_file_open(struct ffs_file **out_file, const char *filename,
                  uint8_t access_flags);
int ffs_file_seek(struct ffs_file *file, uint32_t offset);
int ffs_file_close(struct ffs_file *file);
int ffs_file_new(struct ffs_inode **out_inode, struct ffs_inode *parent,
                 const char *filename, uint8_t filename_len, int is_dir);

int ffs_format_area(uint16_t area_idx, int is_scratch);
int ffs_format_from_scratch_area(uint16_t area_idx, uint16_t area_id);
int ffs_format_full(const struct ffs_area_desc *area_descs);

int ffs_gc(uint16_t *out_area_idx);
int ffs_gc_until(uint16_t *out_area_idx, uint32_t space);

int ffs_area_desc_validate(const struct ffs_area_desc *area_desc);
int ffs_area_magic_is_set(const struct ffs_disk_area *disk_area);
int ffs_area_is_scratch(const struct ffs_disk_area *disk_area);
void ffs_area_to_disk(struct ffs_disk_area *out_disk_area,
                        const struct ffs_area *area);
uint32_t ffs_area_free_space(const struct ffs_area *area);
int ffs_area_find_corrupt_scratch(uint16_t *out_good_idx,
                                  uint16_t *out_bad_idx);

int ffs_misc_validate_root(void);
int ffs_misc_validate_scratch(void);
int ffs_misc_reset(void);
void ffs_misc_set_max_block_data_size(void);

int ffs_write_to_file(struct ffs_file *file, const void *data, int len);

#define FFS_HASH_FOREACH(object, i)                                     \
    for ((i) = 0; (i) < FFS_HASH_SIZE; (i)++)                           \
        SLIST_FOREACH((object), &ffs_hash[i], fb_hash_next)

#endif

