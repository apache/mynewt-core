#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "ffs/ffs.h"

#define PRINTF(...)

static FILE *file;

const struct ffs_sector_desc *temp_ffs_sectors;
static uint32_t ffs_start_addr;

static void
doerase(int addr, int len)
{
    int rc;
    int i;

    rc = fseek(file, addr, SEEK_SET);
    assert(rc == 0);

    for (i = 0; i < len; i++) {
        fputc(0xff, file);
    }
    fflush(file);
}

static void
ensure_file_open(void)
{
    int expected_sz;
    int sz;
    int i;

    if (file == NULL) {
        ffs_start_addr = temp_ffs_sectors[0].fsd_offset;
        expected_sz = 0;
        for (i = 0; temp_ffs_sectors[i].fsd_length != 0; i++) {
            expected_sz += temp_ffs_sectors[i].fsd_length;
        }

        file = fopen("test.bin", "r+");
        if (file == NULL) {
            sz = -1;
        } else {
            fseek(file, 0, SEEK_END);
            sz = ftell(file);
        }

        if (sz != expected_sz) {
            fclose(file);
            file = fopen("test.bin", "w+");
            doerase(0, expected_sz);
        }
    }
}

int
flash_write(const void *src, uint32_t address, uint32_t length)
{
    const uint8_t *s;
    int c;
    int i;

    ensure_file_open();
    PRINTF("writing %u bytes to 0x%x\n", length, address);
    s = src;
    fseek(file, address - ffs_start_addr, SEEK_SET);
    for (i = 0; i < length; i++) {
        c = fgetc(file);
        assert(c != EOF);
        assert((s[i] & c) == s[i]);
        fseek(file, -1, SEEK_CUR);
        fputc(s[i], file);
    }

    fflush(file);
    return 0;
}

int
flash_read(void *dst, uint32_t address, uint32_t length)
{
    ensure_file_open();
    PRINTF("reading %u bytes from 0x%x\n", length, address);
    fseek(file, address - ffs_start_addr, SEEK_SET);
    fread(dst, length, 1, file);

    return 0;
}

static int
find_sector(uint32_t address)
{
    int i;

    for (i = 0; ; i++) {
        if (temp_ffs_sectors[i].fsd_length == 0) {
            assert(0);
            return -1;
        }

        if (temp_ffs_sectors[i].fsd_offset == address) {
            return i;
        }
    }
}

int
flash_erase_sector(uint32_t address)
{
    uint32_t addr;
    int sector_id;

    ensure_file_open();
    PRINTF("erasing sector at %u\n", address);

    sector_id = find_sector(address);
    doerase(address - ffs_start_addr, temp_ffs_sectors[sector_id].fsd_length);

    return 0;
}

