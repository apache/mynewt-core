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

#include <bsp/bsp.h>
#include <sysflash/sysflash.h>
#include <os/util.h>
#include <tusb.h>
#include <class/msc/msc.h>
#include <class/msc/msc_device.h>
#include <modlog/modlog.h>
#include <hal/hal_flash.h>
#include <hal/hal_flash_int.h>
#include <hal/hal_bsp.h>

typedef enum {
    MEDIUM_NOT_PRESENT,
    REPORT_MEDIUM_CHANGE,
    MEDIUM_RELOAD,
    MEDIUM_PRESENT,
} medium_state_t;

struct lun_data {
    medium_state_t medium_state;
    uint32_t block_count;
    uint16_t block_size;
    /* Flash Id or Flash area id */
    uint8_t flash_id;

    uint32_t flash_start;
    uint32_t flash_end;

    /* buffer for checking if flash is empty */
    uint8_t flash_sector_buffer[512];
};

struct lun_data luns[1] = {
    {
        .medium_state = MEDIUM_PRESENT,
        .block_count = 0,
        .block_size = 512,
    }
};

static uint32_t
flash_read(struct lun_data *lun_data, uint32_t addr, uint8_t *buffer, uint32_t bufsize)
{
#if defined(MYNEWT_VAL_MSC_FLASH_FLASH_ID)
    hal_flash_read(lun_data->flash_id, lun_data->flash_start + addr, buffer, bufsize);
#elif defined(MYNEWT_VAL_MSC_FLASH_FLASH_AREA_ID)
    const struct flash_area *fa;
    flash_area_open(MYNEWT_VAL_MSC_FLASH_FLASH_AREA_ID, &fa);
    if (fa) {
        flash_area_read(fa, addr, buffer, bufsize);
        flash_area_close(fa);
    }
#endif
    return bufsize;
}

static uint32_t
flash_write(struct lun_data *lun_data, uint32_t addr, const uint8_t *buffer,
            uint32_t bufsize)
{
#if defined(MYNEWT_VAL_MSC_FLASH_FLASH_ID)
    hal_flash_write(lun_data->flash_id, lun_data->flash_start + addr, buffer, bufsize);
#elif defined(MYNEWT_VAL_MSC_FLASH_FLASH_AREA_ID)
    const struct flash_area *fa;
    flash_area_open(MYNEWT_VAL_MSC_FLASH_FLASH_AREA_ID, &fa);
    if (fa) {
        flash_area_write(fa, addr, buffer, bufsize);
        flash_area_close(fa);
    }
#endif
    return bufsize;
}

static void
flash_erase_if_needed(struct lun_data *lun_data, uint32_t addr, uint32_t size)
{
    int left = size;
    while (left > 0) {
#if defined(MYNEWT_VAL_MSC_FLASH_FLASH_ID)
        if (!hal_flash_isempty(lun_data->flash_id, lun_data->flash_start + addr,
                               lun_data->flash_sector_buffer, 512)) {
            hal_flash_erase(lun_data->flash_id, lun_data->flash_start + addr, 512);
        }
#elif defined(MYNEWT_VAL_MSC_FLASH_FLASH_AREA_ID)
        const struct flash_area *fa;
        flash_area_open(MYNEWT_VAL_MSC_FLASH_FLASH_AREA_ID, &fa);
        if (fa) {
            if (!flash_area_read_is_empty(fa, addr,
                                          lun_data->flash_sector_buffer, 512)) {
                flash_area_erase(fa, addr, 512);
            }
            flash_area_close(fa);
        }
#endif
        left -= 512;
    }
}
#define IN_ARRAY(arr, ix) ((ix) < ARRAY_SIZE(arr))

static struct lun_data *
get_lun_data(uint8_t lun)
{
    return IN_ARRAY(luns, lun) ? luns + lun : NULL;
}

/*
 * Invoked when received SCSI_CMD_INQUIRY
 * Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
 */
void
tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16],
                   uint8_t product_rev[4])
{
    (void)lun;

    const char rev[] = "1.0";
    int len;

    MSC_FLASH_LOG_DEBUG("SCSI inquiry\n");

    len = min(8, sizeof(MYNEWT_VAL_USBD_VENDOR_STRING));
    memcpy((char *)vendor_id, MYNEWT_VAL_USBD_VENDOR_STRING, len);
    len = min(16, sizeof(MYNEWT_VAL_USBD_PRODUCT_STRING));
    strncpy((char *)product_id, MYNEWT_VAL_USBD_PRODUCT_STRING, len);
    memcpy(product_rev, rev, strlen(rev));
}

/*
 * Invoked when received Test Unit Ready command.
 * return true allowing host to read/write this LUN e.g SD card inserted
 */
bool
tud_msc_test_unit_ready_cb(uint8_t lun)
{
    bool ret;

    ret = lun < ARRAY_SIZE(luns) && luns[lun].block_count > 0;

    return ret;
}

/*
 * Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
 * Application update block count and block size
 */
void
tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
    *block_count = 0;
    *block_size = 0;
    if (lun < ARRAY_SIZE(luns)) {
        *block_count = luns[lun].block_count;
        *block_size = luns[lun].block_size;
    }

    MSC_FLASH_LOG_DEBUG("SCSI capacity block size: %d, block count: %d\n",
                        *block_size, *block_count);
}

/*
 * Invoked when received Start Stop Unit command
 * - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
 * - Start = 1 : active mode, if load_eject = 1 : load disk storage
 */
bool
tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
    struct lun_data *lun_data = get_lun_data(lun);

    (void)power_condition;

    MSC_FLASH_LOG_DEBUG("Start: %d eject: %d\n", start, load_eject);

    if (load_eject) {
        if (start) {
        } else {
            lun_data->medium_state = MEDIUM_NOT_PRESENT;
        }
    }

    return true;
}

/*
 * Callback invoked when received READ10 command.
 * Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
 */
int32_t
tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer,
                  uint32_t bufsize)
{
    struct lun_data *lun_data = get_lun_data(lun);

    MSC_FLASH_LOG_DEBUG("read10(%d, %d, buf, %d)", lba, offset, bufsize);

    /* Check if disk is even present */
    if (lun_data == NULL || lun_data->medium_state != MEDIUM_PRESENT) {
        return 0;
    }

    /* If MBR was selected add marker and partition type */
    if (MYNEWT_VAL_MSC_FLASH_MBR == 1 && lba == 0) {
        uint8_t *buf = buffer;

        memset(buffer, 0, bufsize);
        buf[450] = 0x81;
        buf[510] = 0x55;
        buf[511] = 0xAA;
    } else {
        flash_read(lun_data, (lba - MYNEWT_VAL_MSC_FLASH_MBR) * 512, buffer, bufsize);
    }

    return bufsize;
}

/*
 * Callback invoked when received WRITE10 command.
 * Process data in buffer to disk's storage and return number of written bytes
 */
int32_t
tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer,
                   uint32_t bufsize)
{
    struct lun_data *lun_data = get_lun_data(lun);
    uint32_t flash_addr;

    MSC_FLASH_LOG_DEBUG("write10(%u, %u, buf, %u)", (unsigned)lba,
                        (unsigned)offset, (unsigned)bufsize);

    if (lun_data == NULL || lun_data->medium_state != MEDIUM_PRESENT ||
        offset != 0 || bufsize != lun_data->block_size) {
        return -1;
    }

    if (lba >= MYNEWT_VAL_MSC_FLASH_MBR) {
        flash_addr = (lba - MYNEWT_VAL_MSC_FLASH_MBR) * 512;
        flash_erase_if_needed(lun_data, flash_addr, bufsize);
        flash_write(lun_data, flash_addr, buffer, bufsize);
    }

    return bufsize;
}

/*
 * Callback invoked when received an SCSI command not in built-in list below
 * - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
 * - READ10 and WRITE10 has their own callbacks
 */
int32_t
tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize)
{
    void const *response = NULL;
    int32_t resplen = 0;

    /* most scsi handled is input */
    bool in_xfer = true;

    MSC_FLASH_LOG_INFO("SCSI cmd 0x%02X\n", scsi_cmd[0]);

    switch (scsi_cmd[0]) {
    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
        /* Host is about to read/write etc ... better not to disconnect disk */
        resplen = -1;
        break;

    default:
        /* Set Sense = Invalid Command Operation */
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

        /* negative means error -> tinyusb could stall and/or response with failed status */
        resplen = -1;
        break;
    }

    /* return resplen must not larger than bufsize */
    if (resplen > bufsize) {
        resplen = bufsize;
    }

    if (response && (resplen > 0)) {
        if (in_xfer) {
            memcpy(buffer, response, resplen);
        } else {
            /* SCSI output */
        }
    }

    return resplen;
}

#if MYNEWT_VAL_BOOTLOADER
void
boot_preboot(void)
{
    tinyusb_start();
}
#endif

void
msc_flash_init(void)
{
#if defined(MYNEWT_VAL_MSC_FLASH_FLASH_ID)
    /* Flash ID as defined in bsp.yml */
    int flash_id = MYNEWT_VAL_MSC_FLASH_FLASH_ID;
    uint32_t start_address;
    uint32_t sector_size;
    uint32_t end_address;
    uint32_t flash_size;
    const struct hal_flash *hf = hal_bsp_flash_dev(flash_id);

    if (hf) {
        hal_flash_sector_info(flash_id, 0, &start_address, &sector_size);
        start_address += MYNEWT_VAL_MSC_FLASH_FLASH_OFFSET;
        hal_flash_sector_info(flash_id, hf->hf_sector_cnt - 1, &end_address,
                              &sector_size);
        end_address += sector_size;
        flash_size = end_address - start_address;
        luns[0].flash_id = MYNEWT_VAL_MSC_FLASH_FLASH_ID;
        luns[0].block_count = flash_size / 512 + MYNEWT_VAL_MSC_FLASH_MBR;
        luns[0].block_size = 512;
        luns[0].flash_start = start_address;
        luns[0].flash_end = end_address;
    }
#elif defined(MYNEWT_VAL_MSC_FLASH_FLASH_AREA_ID)
    int flash_area_id = MYNEWT_VAL_MSC_FLASH_FLASH_AREA_ID;
    /* Flash ID as defined in bsp.yml */
    const struct flash_area *fa;
    uint32_t start_address;
    uint32_t end_address;
    uint32_t flash_size;

    flash_area_open(flash_area_id, &fa);

    if (fa) {
        start_address = fa->fa_off;
        end_address = start_address + fa->fa_size;
        flash_size = end_address - start_address;
        luns[0].flash_id = MYNEWT_VAL_MSC_FLASH_FLASH_AREA_ID;
        luns[0].block_count = flash_size / 512 + MYNEWT_VAL_MSC_FLASH_MBR;
        luns[0].block_size = 512;
        luns[0].flash_start = start_address;
        luns[0].flash_end = end_address;
    }
#endif
}
