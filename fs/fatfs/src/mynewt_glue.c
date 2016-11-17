#include <assert.h>
#include <hal/hal_flash.h>
#include <flash_map/flash_map.h>
#include <stdio.h>

#include <fatfs/diskio.h>

DSTATUS
disk_initialize(BYTE pdrv)
{
    /* Don't need to do anything while using hal_flash */
    return RES_OK;
}

DSTATUS
disk_status(BYTE pdrv)
{
    /* Always OK on native emulated flash */
    return RES_OK;
}

DRESULT
disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
    int rc;
    uint32_t address;
    uint32_t num_bytes;

    /* NOTE: safe to assume sector size as 512 for now, see ffconf.h */
    address = (uint32_t) sector * 512;
    num_bytes = (uint32_t) count * 512;
    rc = hal_flash_read(pdrv, address, (void *) buff, num_bytes);
    if (rc < 0) {
        return STA_NOINIT;
    }

    return RES_OK;
}

DRESULT
disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count)
{
    int rc;
    uint32_t address;
    uint32_t num_bytes;

    /* NOTE: safe to assume sector size as 512 for now, see ffconf.h */
    address = (uint32_t) sector * 512;
    num_bytes = (uint32_t) count * 512;
    rc = hal_flash_write(pdrv, address, (const void *) buff, num_bytes);
    if (rc < 0) {
        return STA_NOINIT;
    }

    return RES_OK;
}

DRESULT
disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
    return RES_OK;
}

/* FIXME: _FS_NORTC=1 because there is not hal_rtc interface */
DWORD
get_fattime(void)
{
    return 0;
}

void
fatfs_pkg_init(void)
{
    /* Nothing to do for now */
}
