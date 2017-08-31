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
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "hal/hal_flash.h"
#include "testutil/testutil.h"
#include "fs/fs.h"
#include "nffs/nffs.h"
#include "nffs_test.h"
#include "nffs_test_priv.h"
#include "nffs_priv.h"

int print_verbose;

void
print_inode_entry(struct nffs_inode_entry *inode_entry, int indent)
{
    struct nffs_inode inode;
    char name[NFFS_FILENAME_MAX_LEN + 1];
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    if (inode_entry == nffs_root_dir) {
        printf("%*s/\n", indent, "");
        return;
    }

    rc = nffs_inode_from_entry(&inode, inode_entry);
    /*
     * Dummy inode
     */
    if (rc == FS_ENOENT) {
        printf("    DUMMY %d\n", rc);
        return;
    }

    nffs_flash_loc_expand(inode_entry->nie_hash_entry.nhe_flash_loc,
                         &area_idx, &area_offset);


    rc = nffs_flash_read(area_idx,
                         area_offset + sizeof (struct nffs_disk_inode),
                         name, inode.ni_filename_len);

    name[inode.ni_filename_len] = '\0';

    printf("%*s%s %ju %jx\n", indent, "", name[0] == '\0' ? "/" : name,
           (uintmax_t) inode.ni_seq,
           (uintmax_t) inode.ni_inode_entry->nie_flags);
}

void
process_inode_entry(struct nffs_inode_entry *inode_entry, int indent)
{
    struct nffs_inode_entry *child;

    print_inode_entry(inode_entry, indent);

    if (nffs_hash_id_is_dir(inode_entry->nie_hash_entry.nhe_id)) {
        SLIST_FOREACH(child, &inode_entry->nie_child_list, nie_sibling_next) {
            process_inode_entry(child, indent + 2);
        }
    }
}

int
print_nffs_flash_inode(struct nffs_area *area, uint32_t off)
{
    struct nffs_disk_inode ndi;
    char filename[128];
    int len;
    int rc;

    rc = hal_flash_read(area->na_flash_id, area->na_offset + off,
                         &ndi, sizeof(ndi));
    assert(rc == 0);

    memset(filename, 0, sizeof(filename));
    len = min(sizeof(filename) - 1, ndi.ndi_filename_len);
    rc = hal_flash_read(area->na_flash_id, area->na_offset + off + sizeof(ndi),
                         filename, len);

    printf("  off %jx %s id %jx flen %ju seq %ju last %jx prnt %jx "
           "flgs %jx %s\n",
           (uintmax_t) off,
           (nffs_hash_id_is_file(ndi.ndi_id) ? "File" :
            (nffs_hash_id_is_dir(ndi.ndi_id) ? "Dir" : "???")),
           (uintmax_t) ndi.ndi_id,
           (uintmax_t) ndi.ndi_filename_len,
           (uintmax_t) ndi.ndi_seq,
           (uintmax_t) ndi.ndi_lastblock_id,
           (uintmax_t) ndi.ndi_parent_id,
           (uintmax_t) ndi.ndi_flags,
           filename);
    return sizeof(ndi) + ndi.ndi_filename_len;
}

int
print_nffs_flash_block(struct nffs_area *area, uint32_t off)
{
    struct nffs_disk_block ndb;
    int rc;

    rc = hal_flash_read(area->na_flash_id, area->na_offset + off,
                        &ndb, sizeof(ndb));
    assert(rc == 0);

    printf("  off %jx Block id %jx len %ju seq %ju prev %jx own ino %jx\n",
           (uintmax_t) off,
           (uintmax_t) ndb.ndb_id,
           (uintmax_t) ndb.ndb_data_len,
           (uintmax_t) ndb.ndb_seq,
           (uintmax_t) ndb.ndb_prev_id,
           (uintmax_t) ndb.ndb_inode_id);
    return sizeof(ndb) + ndb.ndb_data_len;
}

int
print_nffs_flash_object(struct nffs_area *area, uint32_t off)
{
    struct nffs_disk_object ndo;

    hal_flash_read(area->na_flash_id, area->na_offset + off,
                        &ndo.ndo_un_obj, sizeof(ndo.ndo_un_obj));

    if (nffs_hash_id_is_inode(ndo.ndo_disk_inode.ndi_id)) {
        return print_nffs_flash_inode(area, off);

    } else if (nffs_hash_id_is_block(ndo.ndo_disk_block.ndb_id)) {
        return print_nffs_flash_block(area, off);

    } else if (ndo.ndo_disk_block.ndb_id == 0xffffffff) {
        return area->na_length;

    } else {
        return 1;
    }
}

void
print_nffs_flash_areas(int verbose)
{
    struct nffs_area area;
    struct nffs_disk_area darea;
    int off;
    int i;

    for (i = 0; nffs_current_area_descs[i].nad_length != 0; i++) {
        if (i > NFFS_MAX_AREAS) {
            return;
        }
        area.na_offset = nffs_current_area_descs[i].nad_offset;
        area.na_length = nffs_current_area_descs[i].nad_length;
        area.na_flash_id = nffs_current_area_descs[i].nad_flash_id;
        hal_flash_read(area.na_flash_id, area.na_offset, &darea, sizeof(darea));
        area.na_id = darea.nda_id;
        area.na_cur = nffs_areas[i].na_cur;
        if (!nffs_area_magic_is_set(&darea)) {
            printf("Area header corrupt!\n");
        }
        printf("area %d: id %ju %jx-%jx cur %jx len %ju flashid %jx "
               "gc-seq %jd %s%s\n",
               i,
               (uintmax_t)area.na_id,
               (uintmax_t)area.na_offset,
               (uintmax_t)area.na_offset + area.na_length,
               (uintmax_t)area.na_cur,
               (uintmax_t)area.na_length,
               (uintmax_t)area.na_flash_id,
               (uintmax_t)darea.nda_gc_seq,
               nffs_scratch_area_idx == i ? "(scratch)" : "",
               !nffs_area_magic_is_set(&darea) ? "corrupt" : "");
        if (verbose < 2) {
            off = sizeof (struct nffs_disk_area);
            while (off < area.na_length) {
                off += print_nffs_flash_object(&area, off);
            }
        }
    }
}

static int
nffs_hash_fn(uint32_t id)
{
    return id % NFFS_HASH_SIZE;
}

void
print_hashlist(struct nffs_hash_entry *he)
{
    struct nffs_hash_list *list;
    int idx = nffs_hash_fn(he->nhe_id);
    list = nffs_hash + idx;

    SLIST_FOREACH(he, list, nhe_next) {
        printf("hash_entry %s %p: id 0x%jx flash_loc 0x%jx next %p\n",
                   nffs_hash_id_is_inode(he->nhe_id) ? "inode" : "block",
                   he,
                   (uintmax_t)he->nhe_id,
                   (uintmax_t)he->nhe_flash_loc,
                   he->nhe_next.sle_next);
   }
}

void
print_hash(void)
{
    int i;
    struct nffs_hash_entry *he;
    struct nffs_hash_entry *next;
    struct nffs_inode ni;
    struct nffs_disk_inode di;
    struct nffs_block nb;
    struct nffs_disk_block db;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    NFFS_HASH_FOREACH(he, i, next) {
        if (nffs_hash_id_is_inode(he->nhe_id)) {
            printf("hash_entry inode %d %p: id 0x%jx flash_loc 0x%jx "
                   "next %p\n",
                   i,
                   he,
                   (uintmax_t)he->nhe_id,
                   (uintmax_t)he->nhe_flash_loc,
                   he->nhe_next.sle_next);
            if (he->nhe_id == NFFS_ID_ROOT_DIR) {
                continue;
            }
            nffs_flash_loc_expand(he->nhe_flash_loc,
                                  &area_idx, &area_offset);
            rc = nffs_inode_read_disk(area_idx, area_offset, &di);
            if (rc) {
                printf("%d: fail inode read id 0x%jx rc %d\n",
                       i, (uintmax_t)he->nhe_id, rc);
            }
            printf("    Disk inode: id %jx seq %ju parent %jx last %jx "
                   "flgs %jx\n",
                   (uintmax_t)di.ndi_id,
                   (uintmax_t)di.ndi_seq,
                   (uintmax_t)di.ndi_parent_id,
                   (uintmax_t)di.ndi_lastblock_id,
                   (uintmax_t)di.ndi_flags);
            ni.ni_inode_entry = (struct nffs_inode_entry *)he;
            ni.ni_seq = di.ndi_seq; 
            ni.ni_parent = nffs_hash_find_inode(di.ndi_parent_id);
            printf("    RAM inode: entry %p seq %ju parent %p "
                   "filename %s\n",
                   ni.ni_inode_entry,
                   (uintmax_t)ni.ni_seq,
                   ni.ni_parent,
                   ni.ni_filename);

        } else if (nffs_hash_id_is_block(he->nhe_id)) {
            printf("hash_entry block %d %p: id 0x%jx flash_loc 0x%jx "
                   "next %p\n",
                   i,
                   he,
                   (uintmax_t)he->nhe_id,
                   (uintmax_t)he->nhe_flash_loc,
                   he->nhe_next.sle_next);
            rc = nffs_block_from_hash_entry(&nb, he);
            if (rc) {
                printf("%d: fail block read id 0x%jx rc %d\n",
                       i, (uintmax_t)he->nhe_id, rc);
            }
            printf("    block: id %jx seq %ju inode %jx prev %jx\n",
                   (uintmax_t)nb.nb_hash_entry->nhe_id,
                   (uintmax_t)nb.nb_seq, 
                   (uintmax_t)nb.nb_inode_entry->nie_hash_entry.nhe_id, 
                   (uintmax_t)nb.nb_prev->nhe_id);
            nffs_flash_loc_expand(nb.nb_hash_entry->nhe_flash_loc,
                                  &area_idx, &area_offset);
            rc = nffs_block_read_disk(area_idx, area_offset, &db);
            if (rc) {
                printf("%d: fail disk block read id 0x%jx rc %d\n",
                       i, (uintmax_t)nb.nb_hash_entry->nhe_id, rc);
            }
            printf("    disk block: id %jx seq %ju inode %jx prev %jx "
                   "len %ju\n",
                   (uintmax_t)db.ndb_id,
                   (uintmax_t)db.ndb_seq,
                   (uintmax_t)db.ndb_inode_id,
                   (uintmax_t)db.ndb_prev_id,
                   (uintmax_t)db.ndb_data_len);
        } else {
            printf("hash_entry UNKNONN %d %p: id 0x%jx flash_loc 0x%jx "
                   "next %p\n",
                   i,
                   he,
                   (uintmax_t)he->nhe_id,
                   (uintmax_t)he->nhe_flash_loc,
                   he->nhe_next.sle_next);
        }
    }

}

void
nffs_print_object(struct nffs_disk_object *dobj)
{
    struct nffs_disk_inode *di = &dobj->ndo_disk_inode;
    struct nffs_disk_block *db = &dobj->ndo_disk_block;

    if (dobj->ndo_type == NFFS_OBJECT_TYPE_INODE) {
        printf("    %s id %jx seq %ju prnt %jx last %jx\n",
               nffs_hash_id_is_file(di->ndi_id) ? "File" :
                nffs_hash_id_is_dir(di->ndi_id) ? "Dir" : "???",
               (uintmax_t)di->ndi_id,
               (uintmax_t)di->ndi_seq,
               (uintmax_t)di->ndi_parent_id,
               (uintmax_t)di->ndi_lastblock_id);
    } else if (dobj->ndo_type != NFFS_OBJECT_TYPE_BLOCK) {
        printf("    %s: id %jx seq %ju ino %jx prev %jx len %ju\n",
               nffs_hash_id_is_block(db->ndb_id) ? "Block" : "Block?",
               (uintmax_t)db->ndb_id,
               (uintmax_t)db->ndb_seq,
               (uintmax_t)db->ndb_inode_id,
               (uintmax_t)db->ndb_prev_id,
               (uintmax_t)db->ndb_data_len);
    }
}

void
print_nffs_hash_block(struct nffs_hash_entry *he, int verbose)
{
    struct nffs_block nb = { 0 };
    struct nffs_disk_block db = { 0 };
    uint32_t area_offset = 0;
    uint8_t area_idx = 0;
    int rc;

    if (he == NULL) {
        return;
    }
    if (!nffs_hash_entry_is_dummy(he)) {
        nffs_flash_loc_expand(he->nhe_flash_loc,
                              &area_idx, &area_offset);
        rc = nffs_block_read_disk(area_idx, area_offset, &db);
        if (rc) {
            printf("%p: fail block read id 0x%jx rc %d\n",
                   he, (uintmax_t)he->nhe_id, rc);

        }
        nb.nb_hash_entry = he;
        nb.nb_seq = db.ndb_seq;
        if (db.ndb_inode_id != NFFS_ID_NONE) {
            nb.nb_inode_entry = nffs_hash_find_inode(db.ndb_inode_id);
        } else {
            nb.nb_inode_entry = (void*)db.ndb_inode_id;
        }
        if (db.ndb_prev_id != NFFS_ID_NONE) {
            nb.nb_prev = nffs_hash_find_block(db.ndb_prev_id);
        } else {
            nb.nb_prev = (void*)db.ndb_prev_id;
        }
        nb.nb_data_len = db.ndb_data_len;
    }
    if (!verbose) {
        printf("%s%s id %jx idx/off %ju/%jx seq %ju ino %jx prev %jx "
               "len %ju\n",
               nffs_hash_entry_is_dummy(he) ? "Dummy " : "",
               nffs_hash_id_is_block(he->nhe_id) ? "Block" : "Unknown",
               (uintmax_t)he->nhe_id,
               (uintmax_t)area_idx,
               (uintmax_t)area_offset,
               (uintmax_t)nb.nb_seq,
               (uintmax_t)(nb.nb_inode_entry ? nb.nb_inode_entry->nie_hash_entry.nhe_id : 0),
               (uintmax_t)db.ndb_prev_id,
               (uintmax_t)db.ndb_data_len);
        return;
    }
    printf("%s%s id %jx loc %jx/%jx %jx ent %p\n",
           nffs_hash_entry_is_dummy(he) ? "Dummy " : "",
           nffs_hash_id_is_block(he->nhe_id) ? "Block:" : "Unknown:",
           (uintmax_t)he->nhe_id,
           (uintmax_t)area_idx,
           (uintmax_t)area_offset,
           (uintmax_t)he->nhe_flash_loc,
           he);
    if (nb.nb_inode_entry) {
        printf("  Ram: ent %p seq %ju ino %p prev %p len %ju\n",
               nb.nb_hash_entry,
               (uintmax_t)nb.nb_seq,
               nb.nb_inode_entry,
               nb.nb_prev,
               (uintmax_t)nb.nb_data_len);
    }
    if (db.ndb_id) {
        printf("  Disk %s id %jx seq %ju ino %jx prev %jx len %ju\n",
               nffs_hash_id_is_block(db.ndb_id) ? "Block:" : "???:",
               (uintmax_t)db.ndb_id,
               (uintmax_t)db.ndb_seq,
               (uintmax_t)db.ndb_inode_id,
               (uintmax_t)db.ndb_prev_id,
               (uintmax_t)db.ndb_data_len);
    }
}

void
print_nffs_hash_inode(struct nffs_hash_entry *he, int verbose)
{
    struct nffs_inode ni = { 0 };
    struct nffs_disk_inode di = { 0 };
    struct nffs_inode_entry *nie = (struct nffs_inode_entry*)he;
    int cached_name_len;
    uint32_t area_offset = 0;
    uint8_t area_idx = 0;
    int rc = 0;

    if (he == NULL) {
        return;
    }
    if (!nffs_hash_entry_is_dummy(he)) {
        nffs_flash_loc_expand(he->nhe_flash_loc,
                              &area_idx, &area_offset);
        rc = nffs_inode_read_disk(area_idx, area_offset, &di);
        if (rc) {
            printf("Entry %p: fail inode read id 0x%jx rc %d\n",
                   he, (uintmax_t)he->nhe_id, rc);
        }
        ni.ni_inode_entry = (struct nffs_inode_entry *)he;
        ni.ni_seq = di.ndi_seq; 
        if (di.ndi_parent_id != NFFS_ID_NONE) {
            ni.ni_parent = nffs_hash_find_inode(di.ndi_parent_id);
        } else {
            ni.ni_parent = NULL;
        }
        if (ni.ni_filename_len > NFFS_SHORT_FILENAME_LEN) {
            cached_name_len = NFFS_SHORT_FILENAME_LEN;
        } else {
            cached_name_len = ni.ni_filename_len;
        }
        if (cached_name_len != 0) {
            rc = nffs_flash_read(area_idx, area_offset + sizeof di,
                         ni.ni_filename, cached_name_len);
            if (rc != 0) {
                printf("entry %p: fail filename read id 0x%jx rc %d\n",
                       he, (uintmax_t)he->nhe_id, rc);
                return;
            }
        }
    }
    if (!verbose) {
        printf("%s%s id %jx idx/off %jx/%jx seq %ju prnt %jx last %jx "
               "flags %jx",
               nffs_hash_entry_is_dummy(he) ? "Dummy " : "",
               nffs_hash_id_is_file(he->nhe_id) ? "File" :
                he->nhe_id == NFFS_ID_ROOT_DIR ? "**ROOT Dir" : 
                nffs_hash_id_is_dir(he->nhe_id) ? "Dir" : "Inode",

               (uintmax_t)he->nhe_id,
               (uintmax_t)area_idx,
               (uintmax_t)area_offset,
               (uintmax_t)ni.ni_seq,
               (uintmax_t)di.ndi_parent_id,
               (uintmax_t)di.ndi_lastblock_id,
               (uintmax_t)nie->nie_flags);
        if (ni.ni_inode_entry) {
            printf(" ref %ju\n", (uintmax_t)ni.ni_inode_entry->nie_refcnt);
        } else {
            printf("\n");
        }
        return;
    }
    printf("%s%s id %jx loc %jx/%jx %jx entry %p\n",
           nffs_hash_entry_is_dummy(he) ? "Dummy " : "",
           nffs_hash_id_is_file(he->nhe_id) ? "File:" :
            he->nhe_id == NFFS_ID_ROOT_DIR ? "**ROOT Dir:" : 
            nffs_hash_id_is_dir(he->nhe_id) ? "Dir:" : "Inode:",
           (uintmax_t)he->nhe_id,
           (uintmax_t)area_idx,
           (uintmax_t)area_offset,
           (uintmax_t)he->nhe_flash_loc,
           he);
    if (ni.ni_inode_entry) {
        printf("  ram: ent %p seq %ju prnt %p lst %p ref %ju flgs %jx nm %s\n",
               ni.ni_inode_entry,
               (uintmax_t)ni.ni_seq,
               ni.ni_parent,
               ni.ni_inode_entry->nie_last_block_entry,
               (uintmax_t)ni.ni_inode_entry->nie_refcnt,
               (uintmax_t)ni.ni_inode_entry->nie_flags,
               ni.ni_filename);
    }
    if (rc == 0) {
        printf("  Disk %s: id %jx seq %ju prnt %jx lst %jx flgs %jx\n",
               nffs_hash_id_is_file(di.ndi_id) ? "File" :
                nffs_hash_id_is_dir(di.ndi_id) ? "Dir" : "???",
               (uintmax_t)di.ndi_id,
               (uintmax_t)di.ndi_seq,
               (uintmax_t)di.ndi_parent_id,
               (uintmax_t)di.ndi_lastblock_id,
               (uintmax_t)di.ndi_flags);
    }
}

void
print_hash_entries(int verbose)
{
    int i;
    struct nffs_hash_entry *he;
    struct nffs_hash_entry *next;

    printf("\nnffs_hash_entries:\n");
    for (i = 0; i < NFFS_HASH_SIZE; i++) {
        he = SLIST_FIRST(nffs_hash + i);
        while (he != NULL) {
            next = SLIST_NEXT(he, nhe_next);
            if (nffs_hash_id_is_inode(he->nhe_id)) {
                print_nffs_hash_inode(he, verbose);
            } else if (nffs_hash_id_is_block(he->nhe_id)) {
                print_nffs_hash_block(he, verbose);
            } else {
                printf("UNKNOWN type hash entry %d: id 0x%jx loc 0x%jx\n",
                       i,
                       (uintmax_t)he->nhe_id,
                       (uintmax_t)he->nhe_flash_loc);
            }
            he = next;
        }
    }
}

void
print_nffs_hashlist(int verbose)
{
    struct nffs_hash_entry *he;
    struct nffs_hash_entry *next;
    int i;

    NFFS_HASH_FOREACH(he, i, next) {
        if (nffs_hash_id_is_inode(he->nhe_id)) {
            print_nffs_hash_inode(he, verbose);
        } else if (nffs_hash_id_is_block(he->nhe_id)) {
            print_nffs_hash_block(he, verbose);
        } else {
            printf("UNKNOWN type hash entry %d: id 0x%jx loc 0x%jx\n",
                   i,
                   (uintmax_t)he->nhe_id,
                   (uintmax_t)he->nhe_flash_loc);
        }
    }
}

void
printfs()
{
    if (nffs_misc_ready()) {
        printf("NFFS directory:\n");
        process_inode_entry(nffs_root_dir, print_verbose);

        printf("\nNFFS hash list:\n");
        print_nffs_hashlist(print_verbose);
    }
    printf("\nNFFS flash areas:\n");
    print_nffs_flash_areas(print_verbose);
}
