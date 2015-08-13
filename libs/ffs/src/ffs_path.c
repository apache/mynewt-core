#include <assert.h>
#include <string.h>
#include "ffs/ffs.h"
#include "ffs_priv.h"

int
ffs_path_parse_next(struct ffs_path_parser *parser)
{
    const char *slash_start;
    int token_end;
    int token_len;

    if (parser->fpp_token_type == FFS_PATH_TOKEN_LEAF) {
        return FFS_EINVAL;
    }

    slash_start = strchr(parser->fpp_path + parser->fpp_off, '/');
    if (slash_start == NULL) {
        if (parser->fpp_token_type == FFS_PATH_TOKEN_NONE) {
            return FFS_EINVAL;
        }
        parser->fpp_token_type = FFS_PATH_TOKEN_LEAF;
        token_len = strlen(parser->fpp_path + parser->fpp_off);
    } else {
        parser->fpp_token_type = FFS_PATH_TOKEN_BRANCH;
        token_end = slash_start - parser->fpp_path;
        token_len = token_end - parser->fpp_off;
    }

    if (token_len > FFS_FILENAME_MAX_LEN) {
        return FFS_EINVAL;
    }

    parser->fpp_token = parser->fpp_path + parser->fpp_off;
    parser->fpp_token_len = token_len;
    parser->fpp_off += token_len + 1;

    return 0;
}

void
ffs_path_parser_new(struct ffs_path_parser *parser, const char *path)
{
    parser->fpp_token_type = FFS_PATH_TOKEN_NONE;
    parser->fpp_path = path;
    parser->fpp_off = 0;
}

static int
ffs_path_find_child(struct ffs_inode_entry *parent,
                    const char *name, int name_len,
                    struct ffs_inode_entry **out_inode_entry)
{
    struct ffs_inode_entry *cur;
    struct ffs_inode inode;
    int cmp;
    int rc;

    SLIST_FOREACH(cur, &parent->fie_child_list, fie_sibling_next) {
        rc = ffs_inode_from_entry(&inode, cur);
        if (rc != 0) {
            return rc;
        }

        rc = ffs_inode_filename_cmp_ram(&inode, name, name_len, &cmp);
        if (rc != 0) {
            return rc;
        }

        if (cmp == 0) {
            *out_inode_entry = cur;
            return 0;
        }
        if (cmp > 0) {
            break;
        }
    }

    return FFS_ENOENT;
}

int
ffs_path_find(struct ffs_path_parser *parser,
              struct ffs_inode_entry **out_inode_entry,
              struct ffs_inode_entry **out_parent)
{
    struct ffs_inode_entry *parent;
    struct ffs_inode_entry *inode_entry;
    int rc;

    *out_inode_entry = NULL;
    if (out_parent != NULL) {
        *out_parent = NULL;
    }

    inode_entry = NULL;
    while (1) {
        parent = inode_entry;
        rc = ffs_path_parse_next(parser);
        if (rc != 0) {
            return rc;
        }

        switch (parser->fpp_token_type) {
        case FFS_PATH_TOKEN_BRANCH:
            if (parent == NULL) {
                /* First directory must be root. */
                if (parser->fpp_token_len != 0) {
                    return FFS_ENOENT;
                }

                inode_entry = ffs_root_dir;
            } else {
                /* Ignore empty intermediate directory names. */
                if (parser->fpp_token_len == 0) {
                    break;
                }

                rc = ffs_path_find_child(parent, parser->fpp_token,
                                         parser->fpp_token_len, &inode_entry);
                if (rc != 0) {
                    goto done;
                }
            }
            break;
        case FFS_PATH_TOKEN_LEAF:
            if (parent == NULL) {
                /* First token must be root directory. */
                return FFS_ENOENT;
            }

            rc = ffs_path_find_child(parent, parser->fpp_token,
                                     parser->fpp_token_len, &inode_entry);
            goto done;
        }
    }

done:
    *out_inode_entry = inode_entry;
    if (out_parent != NULL) {
        *out_parent = parent;
    }
    return rc;
}

int
ffs_path_find_inode_entry(const char *filename,
                          struct ffs_inode_entry **out_inode_entry)
{
    struct ffs_path_parser parser;
    int rc;

    ffs_path_parser_new(&parser, filename);
    rc = ffs_path_find(&parser, out_inode_entry, NULL);

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
ffs_path_unlink(const char *path)
{
    struct ffs_inode_entry *inode_entry;
    struct ffs_inode inode;
    int rc;

    rc = ffs_path_find_inode_entry(path, &inode_entry);
    if (rc != 0) {
        return rc;
    }

    rc = ffs_inode_from_entry(&inode, inode_entry);
    if (rc != 0) {
        return rc;
    }

    rc = ffs_inode_unlink(&inode);
    if (rc != 0) {
        return rc;
    }

    return 0;
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
ffs_path_rename(const char *from, const char *to)
{
    struct ffs_path_parser parser;
    struct ffs_inode_entry *from_inode_entry;
    struct ffs_inode_entry *to_inode_entry;
    struct ffs_inode_entry *from_parent;
    struct ffs_inode_entry *to_parent;
    struct ffs_inode inode;
    int rc;

    ffs_path_parser_new(&parser, from);
    rc = ffs_path_find(&parser, &from_inode_entry, &from_parent);
    if (rc != 0) {
        return rc;
    }

    ffs_path_parser_new(&parser, to);
    rc = ffs_path_find(&parser, &to_inode_entry, &to_parent);
    switch (rc) {
    case 0:
        /* The user is clobbering something with the rename. */
        if (ffs_hash_id_is_dir(from_inode_entry->fie_hash_entry.fhe_id) ^
            ffs_hash_id_is_dir(to_inode_entry->fie_hash_entry.fhe_id)) {

            /* Cannot clobber one type of file with another. */
            return FFS_EINVAL;
        }

        rc = ffs_inode_from_entry(&inode, to_inode_entry);
        if (rc != 0) {
            return rc;
        }

        rc = ffs_inode_unlink(&inode);
        if (rc != 0) {
            return rc;
        }
        break;

    case FFS_ENOENT:
        assert(to_parent != NULL);
        if (parser.fpp_token_type != FFS_PATH_TOKEN_LEAF) {
            /* Intermediate directory doesn't exist. */
            return FFS_EINVAL;
        }
        break;

    default:
        return rc;
    }

    if (from_parent != to_parent) {
        if (from_parent != NULL) {
            rc = ffs_inode_from_entry(&inode, from_inode_entry);
            if (rc != 0) {
                return rc;
            }
            ffs_inode_remove_child(&inode);
        }
        if (to_parent != NULL) {
            rc = ffs_inode_add_child(to_parent, from_inode_entry);
            if (rc != 0) {
                return rc;
            }
        }
    }

    rc = ffs_inode_rename(from_inode_entry, to_parent, parser.fpp_token);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/**
 * Creates a new directory at the specified path.
 *
 * @param path                  The path of the directory to create.
 *
 * @return                      0 on success;
 *                              FFS_EEXIST if there is another file or
 *                                  directory at the specified path.
 *                              FFS_ENONT if a required intermediate directory
 *                                  does not exist.
 */
int
ffs_path_new_dir(const char *path)
{
    struct ffs_path_parser parser;
    struct ffs_inode_entry *parent;
    struct ffs_inode_entry *inode;
    int rc;

    ffs_path_parser_new(&parser, path);
    rc = ffs_path_find(&parser, &inode, &parent);
    if (rc == 0) {
        return FFS_EEXIST;
    }
    if (rc != FFS_ENOENT) {
        return rc;
    }
    if (parser.fpp_token_type != FFS_PATH_TOKEN_LEAF || parent == NULL) {
        return FFS_ENOENT;
    }

    rc = ffs_file_new(parent, parser.fpp_token, parser.fpp_token_len, 1, 
                      &inode);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
