/**
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

/**
 * XXX
 * This is a hack of a tool which prints the structure of an nffs file system.
 * It needs to be rewritten properly.
 */

#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <dirent.h>
#include <strings.h>
#include "../src/nffs_priv.h"
#include <os/queue.h>
#include <fs/fs.h>
#include <bsp/bsp.h>
#include <nffs/nffs.h>
#include <hal/hal_flash.h>
#include <hal/flash_map.h>
#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif

#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

static struct log_handler nffs_log_console_handler;
struct log nffs_log;
static const char *copy_in_dir;
static const char *progname;

char *file_flash_area;
int file_scratch_idx;

#define MAX_AREAS	16
static struct nffs_area_desc area_descs[MAX_AREAS];

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

static void
printfs(void)
{
    printf("\n\nNFFS contents:\n");
    process_inode_entry(nffs_root_dir, 0);
    printf("\nNFFS areas:\n");
    print_nffs_areas();
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
                printf("  error code %d ", rc);
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
            printf("Skipping %s\n", src_name);
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

static int
print_flash_inode(struct nffs_area_desc *area, uint32_t off)
{
    struct nffs_disk_inode ndi;
    char filename[128];
    int len;
    int rc;

    rc = file_flash_read(area->nad_offset + off, &ndi, sizeof(ndi));
    assert(rc == 0);

    memset(filename, 0, sizeof(filename));
    len = min(sizeof(filename) - 1, ndi.ndi_filename_len);
    rc = file_flash_read(area->nad_offset + off + sizeof(ndi), filename, len);

	printf("  off %x flen %d %s id %x seq %d prnt %x lstb %x %s\n",
		   off, ndi.ndi_filename_len,
		   (nffs_hash_id_is_file(ndi.ndi_id) ? "File" :
			(nffs_hash_id_is_dir(ndi.ndi_id) ? "Dir" : "???")),
		   ndi.ndi_id, ndi.ndi_seq, ndi.ndi_parent_id,
		   ndi.ndi_lastblock_id, filename);
    return sizeof(ndi) + ndi.ndi_filename_len;
}

static int
print_flash_block(struct nffs_area_desc *area, uint32_t off)
{
    struct nffs_disk_block ndb;
    int rc;

	rc = file_flash_read(area->nad_offset + off, &ndb, sizeof(ndb));
    assert(rc == 0);

    printf("  off %x len %d Block id %x seq %d prev %x own ino %x\n",
		   off, ndb.ndb_data_len, ndb.ndb_id, ndb.ndb_seq,
		   ndb.ndb_prev_id, ndb.ndb_inode_id);
    return sizeof(ndb) + ndb.ndb_data_len;
}

static int
print_flash_object(struct nffs_area_desc *area, uint32_t off)
{
    uint32_t magic;

	file_flash_read(area->nad_offset + off, &magic, sizeof(magic));
    switch (magic) {
    case NFFS_INODE_MAGIC:
        return print_flash_inode(area, off);

    case NFFS_BLOCK_MAGIC:
        return print_flash_block(area, off);
        break;

    case 0xffffffff:
    default:
        return 1;
    }
}

void
print_file_areas()
{
    struct nffs_area_desc *area;
    int off;
    int i;

    for (i = 0; i < nffs_num_areas; i++) {
		area = &area_descs[i];
        printf("%d: id:%d 0x%x - 0x%x %s\n",
			   i, area->nad_flash_id, area->nad_offset,
			   area->nad_offset + area->nad_length,
			   (i == file_scratch_idx ? "(scratch)" : ""));
		off = sizeof (struct nffs_disk_area);
		while (off < area->nad_length) {
			off += print_flash_object(area, off);
		}
    }
}

int
file_area_init(char *flash_area, size_t size)
{
	char *daptr;		/* Disk Area Pointer */
	char *eoda;			/* End Of Disk Area */
	struct nffs_disk_area *nda;
	int nad_cnt = 0;	/* Nffs Area Descriptor count */

	daptr = flash_area;
	eoda = flash_area + size;
	while (daptr < eoda) {
		if (nffs_area_magic_is_set((struct nffs_disk_area*)daptr)) {
			nda = (struct nffs_disk_area*)daptr;
			area_descs[nad_cnt].nad_offset = (daptr - flash_area);
			area_descs[nad_cnt].nad_length = nda->nda_length;
			area_descs[nad_cnt].nad_flash_id = nda->nda_id;
			if (nda->nda_id == 0xff)
				file_scratch_idx = nad_cnt;
			printf("area %d: off %d len %d flshid %x gc-seq %d id %x ver %d %s\n",
				   nad_cnt,
				   area_descs[nad_cnt].nad_offset,
				   area_descs[nad_cnt].nad_length,
				   area_descs[nad_cnt].nad_flash_id,
				   nda->nda_gc_seq, nda->nda_id, nda->nda_ver,
				   nda->nda_id == 0xff ? "(scratch)" : "");

			nad_cnt++;
			daptr = daptr + nda->nda_length;
		} else {
			daptr++;
		}
	}
	nffs_num_areas = nad_cnt;
	return 0;
}

static void
usage(int rc)
{
    printf("%s [-c]|[-d dir][-s][-f flash_file]\n", progname);
    printf("  Tool for operating on simulator flash image file\n");
    printf("   -c: ...\n");
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

    while ((ch = getopt(argc, argv, "c:d:f:s")) != -1) {
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
        case '?':
        default:
            usage(0);
        }
    }

    os_init();
	if (standalone == 0) {
		rc = flash_area_to_nffs_desc(FLASH_AREA_NFFS, &cnt, area_descs);
		assert(rc == 0);
	}

    rc = hal_flash_init();
    assert(rc == 0);

    rc = nffs_init();
    assert(rc == 0);

	log_init();
	log_console_handler_init(&nffs_log_console_handler);
	log_register("nffs-log", &nffs_log, &nffs_log_console_handler);

	if (standalone) {
		fd = open(native_flash_file, O_RDWR);
		if ((rc = fstat(fd, &st)))
			perror("fstat failed");
		if ((file_flash_area = mmap(0, (size_t)8192, PROT_READ,
							   MAP_FILE|MAP_SHARED, fd, 0)) == MAP_FAILED) {
			perror("%s mmap failed");
		}

		rc = file_area_init(file_flash_area, st.st_size);

		print_file_areas();

		return 0;
	}

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
