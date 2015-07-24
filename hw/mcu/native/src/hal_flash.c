#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>

static FILE *file;

static const struct flash_sector_desc {
    uint32_t fsd_offset;
    uint32_t fsd_length;
} flash_sector_descs[] = {
    { 0x00000000, 16 * 1024 },
    { 0x00004000, 16 * 1024 },
    { 0x00008000, 16 * 1024 },
    { 0x0000c000, 16 * 1024 },
    { 0x00010000, 64 * 1024 },
    { 0x00020000, 128 * 1024 },
    { 0x00040000, 128 * 1024 },
    { 0x00060000, 128 * 1024 },
    { 0x00080000, 128 * 1024 },
    { 0x000a0000, 128 * 1024 },
    { 0x000c0000, 128 * 1024 },
    { 0x000e0000, 128 * 1024 },
};

#define FLASH_NUM_SECTORS   (int)(sizeof flash_sector_descs /       \
                                  sizeof flash_sector_descs[0])

static void
flash_native_erase(uint32_t addr, uint32_t len)
{
    static uint8_t buf[256];
    uint32_t end;
    int chunk_sz;
    int rc;

    end = addr + len;
    memset(buf, 0xff, sizeof buf);

    rc = fseek(file, addr, SEEK_SET);
    assert(rc == 0);

    while (addr < end) {
        if (end - addr < sizeof buf) {
            chunk_sz = end - addr;
        } else {
            chunk_sz = sizeof buf;
        }
        rc = fwrite(buf, chunk_sz, 1, file);
        assert(rc == 1);

        addr += chunk_sz;
    }
    fflush(file);
}

static void
flash_native_ensure_file_open(void)
{
    int expected_sz;
    int i;

    if (file == NULL) {
        expected_sz = 0;
        for (i = 0; i < FLASH_NUM_SECTORS; i++) {
            expected_sz += flash_sector_descs[i].fsd_length;
        }

        file = tmpfile();
        assert(file != NULL);
        flash_native_erase(0, expected_sz);
    }
}

int
flash_write(const void *src, uint32_t address, uint32_t length)
{
    const uint8_t *s;
    int c;
    int i;

    flash_native_ensure_file_open();
    s = src;
    fseek(file, address, SEEK_SET);
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
    flash_native_ensure_file_open();
    fseek(file, address, SEEK_SET);
    fread(dst, length, 1, file);

    return 0;
}

static int
find_sector(uint32_t address)
{
    int i;

    for (i = 0; i < FLASH_NUM_SECTORS; i++) {
        if (flash_sector_descs[i].fsd_offset == address) {
            return i;
        }
    }

    return -1;
}

int
flash_erase_sector(uint32_t address)
{
    int sector_id;

    flash_native_ensure_file_open();

    sector_id = find_sector(address);
    if (sector_id == -1) {
        return -1;
    }

    flash_native_erase(address, flash_sector_descs[sector_id].fsd_length);

    return 0;
}

int
flash_erase(uint32_t address, uint32_t num_bytes)
{
    const struct flash_sector_desc *sector;
    uint32_t end;
    int i;

    end = address + num_bytes;

    for (i = 0; i < FLASH_NUM_SECTORS; i++) {
        sector = flash_sector_descs + i;

        if (sector->fsd_offset >= end) {
            return 0;
        }

        if (address >= sector->fsd_offset &&
            address < sector->fsd_offset + sector->fsd_length) {

            flash_erase_sector(sector->fsd_offset);
        }
    }

    return 0;
}

