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
ffs_path_find_child(struct ffs_inode **out_inode,
                    struct ffs_inode *parent,
                    const char *name, int name_len)
{
    struct ffs_inode *cur;
    int str_rc;
    int rc;

    SLIST_FOREACH(cur, &parent->fi_child_list, fi_sibling_next) {
        rc = ffs_inode_filename_cmp(&str_rc, cur, name, name_len);
        if (rc != 0) {
            return rc;
        }

        if (str_rc == 0) {
            *out_inode = cur;
            return 0;
        }
    }

    return FFS_ENOENT;
}

int
ffs_path_find(struct ffs_path_parser *parser, struct ffs_inode **out_inode,
              struct ffs_inode **out_parent)
{
    struct ffs_inode *parent;
    struct ffs_inode *inode;
    int rc;

    *out_inode = NULL;
    if (out_parent != NULL) {
        *out_parent = NULL;
    }

    inode = NULL;
    while (1) {
        parent = inode;
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

                inode = ffs_root_dir;
            } else {
                /* Ignore empty intermediate directory names. */
                if (parser->fpp_token_len == 0) {
                    break;
                }

                rc = ffs_path_find_child(&inode, parent, parser->fpp_token,
                                         parser->fpp_token_len);
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

            rc = ffs_path_find_child(&inode, parent, parser->fpp_token,
                                     parser->fpp_token_len);
            goto done;
        }
    }

done:
    *out_inode = inode;
    if (out_parent != NULL) {
        *out_parent = parent;
    }
    return rc;
}

int
ffs_path_find_inode(struct ffs_inode **out_inode, const char *filename)
{
    struct ffs_path_parser parser;
    int rc;

    ffs_path_parser_new(&parser, filename);
    rc = ffs_path_find(&parser, out_inode, NULL);

    return rc;
}

