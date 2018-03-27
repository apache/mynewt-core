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


#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include "os/mynewt.h"
#include "../src/nffs_priv.h"
#include <fs/fs.h>
#include <bsp/bsp.h>
#include <nffs/nffs.h>
#include <hal/hal_flash.h>
#include <flash_map/flash_map.h>

#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif

#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

struct log nffs_log;
static const char *copy_in_dir;
static const char *progname;
static int print_verbose;

char *file_flash_area;
size_t file_flash_size;
int file_scratch_idx;

#define MAX_AREAS    16
static struct nffs_area_desc area_descs[MAX_AREAS];
int nffs_version;
int force_version;

/** On-disk representation of a version 0 inode (file or directory). */
struct nffs_disk_V0inode {
    uint32_t ndi_magic;         /* NFFS_INODE_MAGIC */
    uint32_t ndi_id;            /* Unique object ID. */
    uint32_t ndi_seq;           /* Sequence number; greater supersedes
                                   lesser. */
    uint32_t ndi_parent_id;     /* Object ID of parent directory inode. */
    uint8_t reserved8;
    uint8_t ndi_filename_len;   /* Length of filename, in bytes. */
    uint16_t ndi_crc16;         /* Covers rest of header and filename. */
    /* Followed by filename. */
};
#define NFFS_DISK_V0INODE_OFFSET_CRC  18

/** On-disk representation of a version 0 data block. */
struct nffs_disk_V0block {
    uint32_t ndb_magic;     /* NFFS_BLOCK_MAGIC */
    uint32_t ndb_id;        /* Unique object ID. */
    uint32_t ndb_seq;       /* Sequence number; greater supersedes lesser. */
    uint32_t ndb_inode_id;  /* Object ID of owning inode. */
    uint32_t ndb_prev_id;   /* Object ID of previous block in file;
                               NFFS_ID_NONE if this is the first block. */
    uint16_t ndb_data_len;  /* Length of data contents, in bytes. */
    uint16_t ndb_crc16;     /* Covers rest of header and data. */
    /* Followed by 'ndb_data_len' bytes of data. */
};
#define NFFS_DISK_V0BLOCK_OFFSET_CRC  22

struct nffs_disk_V0object {
    int ndo_type;
    uint8_t ndo_area_idx;
    uint32_t ndo_offset;
    union {
        struct nffs_disk_V0inode ndo_disk_V0inode;
        struct nffs_disk_V0block ndo_disk_V0block;
    } ndo_un_V0obj;
};

#define ndo_disk_V0inode    ndo_un_V0obj.ndo_disk_V0inode
#define ndo_disk_V0block    ndo_un_V0obj.ndo_disk_V0block

static void usage(int rc);

static void
copyfs(FILE *fp)
{
    uint32_t dst_addr;
    int rc;
    int c;

    dst_addr = area_descs[0].nad_offset;
    while (1) {
        c = getc(fp);
        if (c == EOF) {
            return;
        }

        rc = hal_flash_write(area_descs[0].nad_flash_id, dst_addr, &c, 1);
        assert(rc == 0);

        dst_addr++;
    }
}

static void
print_inode_entry(struct nffs_inode_entry *inode_entry, int indent)
{
    struct nffs_inode inode;
    char name[NFFS_FILENAME_MAX_LEN + 1];
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    rc = nffs_inode_from_entry(&inode, inode_entry);
    /*
     * Dummy inode
     */
    if (rc == FS_ENOENT) {
        printf("    DUMMY %d\n", rc);
        return;
    }
    assert(rc == 0);

    nffs_flash_loc_expand(inode_entry->nie_hash_entry.nhe_flash_loc,
                         &area_idx, &area_offset);

    rc = nffs_flash_read(area_idx,
                         area_offset + sizeof (struct nffs_disk_inode),
                         name, inode.ni_filename_len);
    assert(rc == 0);

    name[inode.ni_filename_len] = '\0';

    printf("%*s%s\n", indent, "", name[0] == '\0' ? "/" : name);
}

static void
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

static int
print_nffs_inode(struct nffs_disk_inode *ndi, int idx, uint32_t off)
{
    char filename[128];
    int len;
    int rc;

    memset(filename, 0, sizeof(filename));
    len = min(sizeof(filename) - 1, ndi->ndi_filename_len);
    rc = nffs_flash_read(idx, off + sizeof(struct nffs_disk_inode),
                         filename, len);
    printf("      %x-%d inode %d/%d",
      off, ndi->ndi_filename_len, ndi->ndi_id, ndi->ndi_seq);
    if (rc == 0)
        printf(" %s\n", filename);
    else
        printf("\n");
    return (sizeof(struct nffs_disk_inode) + ndi->ndi_filename_len);
}

static int
print_nffs_block(struct nffs_disk_block *ndb, int idx, uint32_t off)
{
    printf("      %x-%d block %u/%u belongs to %u\n",
      off, ndb->ndb_data_len, ndb->ndb_id, ndb->ndb_seq, ndb->ndb_inode_id);
    return sizeof(struct nffs_disk_block) + ndb->ndb_data_len;
}

static int
print_nffs_object(int idx, uint32_t off)
{
    struct nffs_disk_object dobj;
    int rc;

    rc = nffs_flash_read(idx, off, &dobj.ndo_un_obj, sizeof(dobj.ndo_un_obj));
    assert(rc == 0);

    if (nffs_hash_id_is_inode(dobj.ndo_disk_inode.ndi_id)) {
        return print_nffs_inode(&dobj.ndo_disk_inode, idx, off);

    } else if (nffs_hash_id_is_block(dobj.ndo_disk_inode.ndi_id)) {
        return print_nffs_block(&dobj.ndo_disk_block, idx, off);

    } else if (dobj.ndo_disk_inode.ndi_id == NFFS_ID_NONE) {
        assert(0);
        return 0;

    } else {
        printf("      %x Corruption\n", off);
        return 1;
    }
}

static void
print_nffs_darea(struct nffs_disk_area *darea)
{
    printf("\tdarea: len %d ver %d gc_seq %d id %x\n",
           darea->nda_length, darea->nda_ver,
           darea->nda_gc_seq, darea->nda_id);
}

static void
print_nffs_area(int idx)
{
    struct nffs_area *area;
    struct nffs_disk_area darea;
    int off;
    int rc;

    area = &nffs_areas[idx];
    rc = nffs_flash_read(idx, 0, &darea, sizeof(darea));
    assert(rc == 0);
    print_nffs_darea(&darea);
    if (!nffs_area_magic_is_set(&darea)) {
        printf("Area header corrupt!\n");
        return;
    }
    /*
     * XXX Enhance to print but not restore unsupported formats
     */
    if (!nffs_area_is_current_version(&darea)) {
        printf("Area format is not supported!\n");
        return;
    }
    off = sizeof (struct nffs_disk_area);
    while (off < area->na_cur) {
        off += print_nffs_object(idx, off);
    }
}

static void
print_nffs_areas(void)
{
    int i;
    struct nffs_area *area;

    for (i = 0; i < nffs_num_areas; i++) {
        if (nffs_scratch_area_idx == i) {
            printf(" sc ");
        } else {
            printf("    ");
        }

        area = &nffs_areas[i];
        printf("%d: cur:%d id:%d 0x%x-0x%x\n",
          i, area->na_cur, area->na_id,
          area->na_offset, area->na_offset + area->na_length);
        print_nffs_area(i);
    }
} 

static int
copy_in_file(char *src, char *dst)
{
    struct fs_file *nf;
    FILE *fp;
    int rc;
    char data[2048];
    int ret = 0;

    rc = fs_open(dst, FS_ACCESS_WRITE, &nf);
    assert(rc == 0);

    fp = fopen(src, "r");
    if (!fp) {
        perror("fopen()");
        assert(0);
    }
    while ((rc = fread(data, 1, sizeof(data), fp))) {
        rc = fs_write(nf, data, rc);
        if (rc) {
            ret = rc;
            break;
        }
    }
    rc = fs_close(nf);
    assert(rc == 0);
    fclose(fp);
    return ret;
}

void
copy_in_directory(const char *src, const char *dst)
{
    DIR *dr;
    struct dirent *entry;
    char src_name[1024];
    char dst_name[1024];
    int rc;

    dr = opendir(src);
    if (!dr) {
        perror("opendir");
        usage(1);
    }
    while ((entry = readdir(dr))) {
        snprintf(src_name, sizeof(src_name), "%s/%s", src, entry->d_name);
        snprintf(dst_name, sizeof(dst_name), "%s/%s", dst, entry->d_name);
        if (entry->d_type == DT_DIR &&
          strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            rc = fs_mkdir(dst_name);
            copy_in_directory(src_name, dst_name);
            assert(rc == 0);
        } else if (entry->d_type == DT_REG) {
            printf("Copying %s\n", dst_name);
            rc = copy_in_file(src_name, dst_name);
            if (rc) {
                if (print_verbose) {
                    printf("  error code %d ", rc);
                }
                switch (rc) {
                case FS_ENOMEM:
                    printf("out of memory\n");
                    break;
                case FS_EFULL:
                    printf("disk is full\n");
                    break;
                default:
                    printf("\n");
                }
                break;
            }
        } else {
            if (print_verbose) {
                printf("Skipping %s\n", src_name);
            }
        }
    }
    closedir(dr);
}

static int
file_flash_read(uint32_t addr, void *dst, int byte_cnt)
{
    memcpy(dst, (void*)file_flash_area + addr, byte_cnt);
    return 0;
}

/*
 * Print NFFS V0 structures
 * XXX could check for CRC errors
 */
static int
print_nffs_flash_V0inode(struct nffs_area_desc *area, uint32_t off)
{
    struct nffs_disk_V0inode ndi;
    char filename[128];
    int len;
    int rc;

    rc = file_flash_read(area->nad_offset + off, &ndi, sizeof(ndi));
    assert(rc == 0);
    assert(nffs_hash_id_is_inode(ndi.ndi_id));

    memset(filename, 0, sizeof(filename));
    len = min(sizeof(filename) - 1, ndi.ndi_filename_len);
    rc = file_flash_read(area->nad_offset + off + sizeof(ndi), filename, len);
    filename[len] = '\0';
    assert(rc == 0);

    printf("   %s off %d id %x flen %d seq %d prnt %x %s\n",
           (nffs_hash_id_is_file(ndi.ndi_id) ? "File" :
            (nffs_hash_id_is_dir(ndi.ndi_id) ? "Dir" : "???")),
           off, ndi.ndi_id, ndi.ndi_filename_len,
           ndi.ndi_seq, ndi.ndi_parent_id, filename);
    return sizeof(struct nffs_disk_V0inode) + ndi.ndi_filename_len;
}

static int
print_nffs_flash_V0block(struct nffs_area_desc *area, uint32_t off)
{
    struct nffs_disk_V0block ndb;
    int rc;

    rc = file_flash_read(area->nad_offset + off, &ndb, sizeof(ndb));
    assert(rc == 0);
    assert(nffs_hash_id_is_block(ndb.ndb_id));
    assert(!nffs_hash_id_is_inode(ndb.ndb_id));

    printf("   Block off %d id %x len %d seq %d prev %x ino %x\n",
           off, ndb.ndb_id, ndb.ndb_data_len, ndb.ndb_seq,
           ndb.ndb_prev_id, ndb.ndb_inode_id);
    return sizeof(struct nffs_disk_V0block) + ndb.ndb_data_len;
}

static int
print_nffs_flash_V0object(struct nffs_area_desc *area, uint32_t off)
{
    uint32_t magic;
    int rc;

    printf("print_nffs_flash_V0object(area:%d off%d)\n", area->nad_flash_id, off);
    rc = file_flash_read(area->nad_offset + off, &magic, sizeof magic);
    assert(rc == 0);

    switch (magic) {
    case NFFS_INODE_MAGIC:
        return print_nffs_flash_V0inode(area, off);

    case NFFS_BLOCK_MAGIC:
        return print_nffs_flash_V0block(area, off);

    case 0xffffffff:
        return area->nad_length;

    default:
        return 1;
    }
}

static int
print_nffs_flash_inode(struct nffs_area_desc *area, uint32_t off)
{
    struct nffs_disk_inode ndi;
    char filename[128];
    int len;
    int rc;
    uint16_t crc16;
    int badcrc = 0;

    rc = file_flash_read(area->nad_offset + off, &ndi, sizeof(ndi));
    assert(rc == 0);

    crc16 = crc16_ccitt(0, (void*)&ndi, NFFS_DISK_INODE_OFFSET_CRC);

    memset(filename, 0, sizeof(filename));
    len = min(sizeof(filename) - 1, ndi.ndi_filename_len);
    rc = file_flash_read(area->nad_offset + off + sizeof(ndi), filename, len);

    crc16 = crc16_ccitt(crc16, filename, ndi.ndi_filename_len);
    if (crc16 != ndi.ndi_crc16) {
        badcrc = 1;
    }

    printf("  off %x %s id %x flen %d seq %d last %x prnt %x flgs %x %s%s\n",
           off,
           (nffs_hash_id_is_file(ndi.ndi_id) ? "File" :
            nffs_hash_id_is_dir(ndi.ndi_id) ? "Dir" : "???"),
           ndi.ndi_id,
           ndi.ndi_filename_len,
           ndi.ndi_seq,
           ndi.ndi_lastblock_id,
           ndi.ndi_parent_id,
           ndi.ndi_flags,
           filename,
           badcrc ? " (Bad CRC!)" : "");
    return sizeof(ndi) + ndi.ndi_filename_len;
}

static int
print_nffs_flash_block(struct nffs_area_desc *area, uint32_t off)
{
    struct nffs_disk_block ndb;
    int rc;
    uint16_t crc16;
    int badcrc = 0;
    int dataover = 0;

    rc = file_flash_read(area->nad_offset + off, &ndb, sizeof(ndb));
    assert(rc == 0);

    if (off + ndb.ndb_data_len > area->nad_length) {
        dataover++;
    } else {
        crc16 = crc16_ccitt(0, (void*)&ndb, NFFS_DISK_BLOCK_OFFSET_CRC);
        crc16 = crc16_ccitt(crc16,
            (void*)(file_flash_area + area->nad_offset + off + sizeof(ndb)),
            ndb.ndb_data_len);
        if (crc16 != ndb.ndb_crc16) {
            badcrc++;
        }
    }

    printf("  off %x Block id %x len %d seq %d prev %x own ino %x%s%s\n",
           off,
           ndb.ndb_id,
           ndb.ndb_data_len,
           ndb.ndb_seq,
           ndb.ndb_prev_id,
           ndb.ndb_inode_id,
           dataover ? " (Bad data length)" : "",
           badcrc ? " (Bad CRC!)" : "");
    if (dataover) {
        return 1;
    }
    return sizeof(ndb) + ndb.ndb_data_len;
}

static int
print_nffs_flash_object(struct nffs_area_desc *area, uint32_t off)
{
    struct nffs_disk_inode *ndi;
    struct nffs_disk_block *ndb;

    ndi = (struct nffs_disk_inode*)(file_flash_area + area->nad_offset + off);
    ndb = (struct nffs_disk_block*)ndi;

    if (nffs_hash_id_is_inode(ndi->ndi_id)) {
        return print_nffs_flash_inode(area, off);

    } else if (nffs_hash_id_is_block(ndb->ndb_id)) {
        return print_nffs_flash_block(area, off);

    } else if (ndb->ndb_id == 0xffffffff) {
        return area->nad_length;

    } else {
        return 1;
    }
}

static void
print_nffs_file_flash(char *flash_area, size_t size)
{
    char *daptr;        /* Disk Area Pointer */
    char *eoda;            /* End Of Disk Area */
    struct nffs_disk_area *nda;
    int nad_cnt = 0;    /* Nffs Area Descriptor count */
    int off;
    int objsz;

    daptr = flash_area;
    eoda = flash_area + size;
    printf("\nNFFS Flash Areas:\n");
    while (daptr < eoda) {
        if (nffs_area_magic_is_set((struct nffs_disk_area*)daptr)) {
            nda = (struct nffs_disk_area*)daptr;
            area_descs[nad_cnt].nad_offset = (daptr - flash_area);
            area_descs[nad_cnt].nad_length = nda->nda_length;
            area_descs[nad_cnt].nad_flash_id = nda->nda_id;
            if (force_version >= 0) {
                nffs_version = force_version;
            } else {
                nffs_version = nda->nda_ver;
            }

            if (nda->nda_id == 0xff)
                file_scratch_idx = nad_cnt;

            printf("Area %d: off %x-%x len %d flshid %x gcseq %d ver %d id %x%s%s\n",
                   nad_cnt,
                   area_descs[nad_cnt].nad_offset,
                   area_descs[nad_cnt].nad_offset +
                                 area_descs[nad_cnt].nad_length,
                   area_descs[nad_cnt].nad_length,
                   area_descs[nad_cnt].nad_flash_id,
                   nda->nda_gc_seq,
                   nda->nda_ver,
                   nda->nda_id,
                   nda->nda_ver != NFFS_AREA_VER ? " (V0)" : "",
                   nad_cnt == file_scratch_idx ? " (Scratch)" : "");

            if (nffs_version == 0) {
                objsz = sizeof (struct nffs_disk_V0object);
            } else {
                objsz = sizeof (struct nffs_disk_object);
            }
            off = sizeof (struct nffs_disk_area);
            while (off + objsz < area_descs[nad_cnt].nad_length) {
                if (nffs_version == 0) {
                    off += print_nffs_flash_V0object(&area_descs[nad_cnt], off);
                } else if (nffs_version == NFFS_AREA_VER) {
                    off += print_nffs_flash_object(&area_descs[nad_cnt], off);
                }
            }
            printf("\n");

            nad_cnt++;
            daptr = daptr + nda->nda_length;
        } else {
            daptr++;
        }
    }
    nffs_num_areas = nad_cnt;
}

static void
printfs(void)
{
    printf("\nNFFS directory:\n");
    process_inode_entry(nffs_root_dir, print_verbose);

    printf("\nNFFS areas:\n");
    print_nffs_areas();

}

static void
usage(int rc)
{
    printf("%s [-v][-c]|[-d dir][-s][-f flash_file]\n", progname);
    printf("  Tool for operating on simulator flash image file\n");
    printf("   -c: ...\n");
    printf("   -v: verbose\n");
    printf("   -d: use dir as root for NFFS portion and create flash image\n");
    printf("   -f: flash_file is the name of the flash image file\n");
    printf("   -s: use flash area layout in flash image file\n");
    exit(rc);
}

int
main(int argc, char **argv)
{
    FILE *fp;
    int fd;
    int rc;
    int ch;
    int cnt;
    struct stat st;
    int standalone = 0;

    progname = argv[0];
    force_version = -1;

    while ((ch = getopt(argc, argv, "c:d:f:sv01")) != -1) {
        switch (ch) {
        case 'c':
            fp = fopen(optarg, "rb");
            assert(fp != NULL);
            copyfs(fp);
            fclose(fp);
            break;
        case 'd':
            copy_in_dir = optarg;
            break;
        case 's':
            standalone++;
            break;
        case 'f':
            native_flash_file = optarg;
            break;
        case 'v':
            print_verbose++;
            break;
        case '0':
            force_version = 0;
            break;
        case '1':
            force_version = 1;
            break;
        case '?':
        default:
            usage(0);
        }
    }

    sysinit();

    log_register("nffs-log", &nffs_log, &log_console_handler, NULL, LOG_SYSLEVEL);

    file_scratch_idx = MAX_AREAS + 1;

    if (standalone) {
        fd = open(native_flash_file, O_RDWR);
        if ((rc = fstat(fd, &st)))
            perror("fstat failed");
        file_flash_size = st.st_size;
        if ((file_flash_area = mmap(0, (size_t)8192, PROT_READ,
                               MAP_FILE|MAP_SHARED, fd, 0)) == MAP_FAILED) {
            perror("%s mmap failed");
        }

        print_nffs_file_flash(file_flash_area, file_flash_size);

        return 0;
    }

    rc = nffs_misc_desc_from_flash_area(MYNEWT_VAL(NFFS_FLASH_AREA), &cnt,
      area_descs);
    assert(rc == 0);

    if (copy_in_dir) {
        /*
         * Build filesystem from contents of directory
         */
        rc = nffs_format(area_descs);
        assert(rc == 0);
        copy_in_directory(copy_in_dir, "");
    } else {
        rc = nffs_detect(area_descs);
        if (rc) {
            printf("nffs_detect() failed\n");
            exit(0);
        }
    }
    printfs();

    return 0;
}
