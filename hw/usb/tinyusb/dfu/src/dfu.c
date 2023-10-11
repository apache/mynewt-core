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

#include <os/mynewt.h>

#include <class/dfu/dfu_device.h>

#include <bsp/bsp.h>
#include <img_mgmt/img_mgmt.h>
#include <tinyusb/tinyusb.h>
#include <hal/hal_gpio.h>

/*
 * If DFU is activated from bootloader it writes to SLOT0.
 * If application code handles DFU it writes to SLOT1 and marks slot as
 * pending.
 */
#define FIRMWARE_SLOT       MYNEWT_VAL(USBD_DFU_SLOT_ID)

struct os_callout delayed_reset_callout;
struct os_callout auto_confirm_callout;

void
delayed_reset_cb(struct os_event *event)
{
    hal_system_reset();
}

/*
 * DFU callbacks
 * Note: alt is used as the partition number, in order to support multiple partitions like FLASH, EEPROM, etc.
 */

/*
 * Invoked right before tud_dfu_download_cb() (state=DFU_DNBUSY) or tud_dfu_manifest_cb() (state=DFU_MANIFEST)
 * Application return timeout in milliseconds (bwPollTimeout) for the next download/manifest operation.
 * During this period, USB host won't try to communicate with us.
 */
uint32_t
tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state)
{
    if (state == DFU_DNBUSY) {
        return MYNEWT_VAL(USBD_DFU_BLOCK_WRITE_TIME);
    }

    /* Since we don't buffer entire image and do any flashing in manifest stage no extra time is needed */
    return 0;
}

static bool
flash_erased(const struct flash_area *fa, uint32_t off, uint32_t size)
{
    uint8_t buf[64];
    uint32_t limit = off + size;
    uint32_t chunk;

    while (off < limit) {
        chunk = min(sizeof(buf), size);
        if (flash_area_read_is_empty(fa, off, buf, chunk)) {
            size -= chunk;
            off += chunk;
        } else {
            break;
        }
    }

    return off >= limit;
}

/*
 * Invoked when received DFU_DNLOAD (wLength > 0) following by DFU_GETSTATUS (state=DFU_DNBUSY) requests
 * This callback could be returned before flashing op is complete (async).
 * Once finished flashing, application must call tud_dfu_finish_flashing()
 */
void
tud_dfu_download_cb(uint8_t alt, uint16_t block_num, const uint8_t *data, uint16_t length)
{
    const struct flash_area *fa;
    uint32_t status = DFU_STATUS_OK;

    if (flash_area_open(FIRMWARE_SLOT, &fa)) {
        status = DFU_STATUS_ERR_ADDRESS;
    } else {
        if (block_num == 0) {
            USBD_DFU_LOG_INFO("Download started\n");
        }
        if (!flash_erased(fa, block_num * CFG_TUD_DFU_XFER_BUFSIZE, length)) {
            USBD_DFU_LOG_DEBUG("Erasing flash 0x%X (0x%X bytes)\n", block_num * CFG_TUD_DFU_XFER_BUFSIZE, length);
            if (flash_area_erase(fa, block_num * CFG_TUD_DFU_XFER_BUFSIZE, length)) {
                USBD_DFU_LOG_ERROR("Flash erase failed\n");
                status = DFU_STATUS_ERR_ERASE;
            }
        }
        if (status == DFU_STATUS_OK) {
            USBD_DFU_LOG_DEBUG("Writing flash 0x%X (0x%X bytes)\n", block_num * CFG_TUD_DFU_XFER_BUFSIZE, length);

            if (flash_area_write(fa, block_num * CFG_TUD_DFU_XFER_BUFSIZE, data, length) < 0) {
                USBD_DFU_LOG_ERROR("Flash write failed\n");
                status = DFU_STATUS_ERR_PROG;
            }
        }
        flash_area_close(fa);
    }

    tud_dfu_finish_flashing(status);
}

/*
 * Invoked when download process is complete, received DFU_DNLOAD (wLength = 0) followed by DFU_GETSTATUS (state = Manifest)
 * Application can do checksum, or actual flashing if buffered entire image previously.
 * Once finished flashing, application must call tud_dfu_finish_flashing()
 */
void
tud_dfu_manifest_cb(uint8_t alt)
{
    (void)alt;
#if !MYNEWT_VAL(BOOT_LOADER) && MYNEWT_VAL(USBD_DFU_MARK_AS_PENDING)
    USBD_DFU_LOG_INFO("Download completed, enter manifestation. settings slot 1 as pending\n");
    img_mgmt_state_set_pending(1, MYNEWT_VAL(USBD_DFU_MARK_AS_CONFIRMED));
#endif

    /*
     * Flashing op for manifest is complete without error.
     * Application can perform checksum, should it fail,
     * use appropriate status such as errVERIFY.
     */
    tud_dfu_finish_flashing(DFU_STATUS_OK);

#if MYNEWT_VAL(USBD_DFU_RESET_AFTER_DOWNLOAD)
    /*
     * Device should reboot after download is complete.
     * It should be done after final DFU_GETSTATUS. But this event is not
     * propagated to application by TinyUSB stack.
     * To satisfy DFU tools lets give USB stack some time to respond to finishing
     * commands before reboot.
     */
    os_callout_init(&delayed_reset_callout, os_eventq_dflt_get(), delayed_reset_cb, NULL);
    os_callout_reset(&delayed_reset_callout, os_time_ms_to_ticks32(MYNEWT_VAL(USBD_DFU_RESET_TIMEOUT)));
#endif
}

void
tud_dfu_detach_cb(void)
{
    /* TODO: implement detach if needed */
}

/**
 * Function called by mcuboot before image verification/swap/and application execution.
 * If GPIO pin is selected and in active state instead of performing normal startup
 * TinyUSB with DFU interface is started allowing for application update.
 */
void
boot_preboot(void)
{
#if MYNEWT_VAL(USBD_DFU_BOOT_PIN) >= 0
    hal_gpio_init_in(MYNEWT_VAL(USBD_DFU_BOOT_PIN), MYNEWT_VAL(USBD_DFU_BOOT_PIN_PULL));
    if (hal_gpio_read(MYNEWT_VAL(USBD_DFU_BOOT_PIN)) == MYNEWT_VAL(USBD_DFU_BOOT_PIN_VALUE)) {
        hal_gpio_deinit(MYNEWT_VAL(USBD_DFU_BOOT_PIN));
        tinyusb_start();
    }
    hal_gpio_deinit(MYNEWT_VAL(USBD_DFU_BOOT_PIN));
#endif
}

void
auto_confirm_cb(struct os_event *event)
{
    img_mgmt_state_confirm();
}

void
dfu_init(void)
{
    os_callout_init(&auto_confirm_callout, os_eventq_dflt_get(),
                    auto_confirm_cb, NULL);

    os_callout_reset(&auto_confirm_callout, OS_TICKS_PER_SEC * MYNEWT_VAL(USBD_DFU_AUTO_CONFIRM_TIME));
}
