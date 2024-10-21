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


// #if MCUBOOT_MYNEWT
// #include <string.h>
// #include <assert.h>
// #include <nrf.h>
// #include <mcu/nrf54h20_rad_hal.h>
// #include <hal/hal_flash_int.h>
// #include <nrfx_ipc.h>
// #include <bootutil/bootutil.h>
// #include <bootutil/image.h>
// #if !MYNEWT_VAL(IPC_ICBMSG)
// #include <ipc_nrf54h20/ipc_nrf54h20.h>
// #endif

// #define NRF5340_NET_VFLASH_SECTOR_SZ 2048

// struct swap_data {
//     uint32_t encryption_key0[4];
//     uint32_t encryption_key1[4];
//     uint32_t swap_size;
//     struct image_trailer trailer;
// };

// static const struct swap_data swap_data_template = {
//     .encryption_key0 = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
//     .encryption_key1 = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
//     .swap_size = 0xffffffff,
//     .trailer = {
//         .swap_type = BOOT_SWAP_TYPE_PERM,
//         .pad1 = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
//         .copy_done = 0xff,
//         .pad2 = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
//         .image_ok = 0x01,
//         .pad3 = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
//         .magic = { 0x77, 0xc2, 0x95, 0xf3, 0x60, 0xd2, 0xef, 0x7f,
//                    0x35, 0x52, 0x50, 0x0f, 0x2c, 0xb6, 0x79, 0x80 },
//     }
// };

// int
// nrf54h20_rad_get_image_hash(uint8_t area_id, uint8_t hash[32])
// {
//     const struct flash_area *fa = NULL;
//     struct image_header header;
//     uint32_t addr;
//     uint32_t limit;
//     struct image_tlv_info info;
//     struct image_tlv tlv;
//     int rc = -1;

//     if (flash_area_open(area_id, &fa) != 0) {
//         goto end;
//     }

//     /* Does slot contain valid image */
//     if (flash_area_read(fa, 0, &header, sizeof(header)) != 0 ||
//         header.ih_magic != IMAGE_MAGIC) {
//         goto end;
//     }

//     addr = header.ih_hdr_size + header.ih_img_size;
//     /* Does image has TLV? (needed for image hash verification) */
//     if (flash_area_read(fa, addr, &info, sizeof(info)) != 0 ||
//         info.it_magic != IMAGE_TLV_INFO_MAGIC) {
//         goto end;
//     }

//     addr += sizeof(info);
//     limit = addr + info.it_tlv_tot;
//     while (addr + sizeof(tlv) + 32 <= limit) {
//         if (flash_area_read(fa, addr, &tlv, sizeof(tlv)) != 0) {
//             goto end;
//         }
//         addr += sizeof(tlv);
//         if (tlv.it_type == IMAGE_TLV_SHA256) {
//             if (tlv.it_len != 32) {
//                 goto end;
//             }
//             rc = flash_area_read(fa, addr, hash, 32);
//             break;
//         }
//         addr += tlv.it_len;
//     }
// end:
//     if (fa) {
//         flash_area_close(fa);
//     }
//     return rc;
// }

// bool
// nrf54h20_rad_images_match(void)
// {
//     uint8_t active_slot_hash[32];
//     uint8_t pending_slot_hash[32];

//     if (nrf54h20_rad_get_image_hash(FLASH_AREA_IMAGE_0, active_slot_hash) == 0 &&
//         nrf54h20_rad_get_image_hash(FLASH_AREA_IMAGE_1, pending_slot_hash) == 0 &&
//         memcmp(pending_slot_hash, active_slot_hash, 32) == 0) {
//         /* Same hash for current and pending slot, act as slot 2 was empty */
//         return true;
//     }
//     return false;
// }

// static int
// nrf54h20_rad_vflash_read(const struct hal_flash *dev, uint32_t address, void *dst,
//                         uint32_t num_bytes)
// {
//     struct nrf54h20_vflash *vflash = (struct nrf54h20_vflash *)dev;
//     uint32_t write_bytes = num_bytes;
//     uint8_t *wp = dst;
//     const struct flash_area *fa;
//     struct swap_data swap_data;

//     if ((vflash->nv_image_size > 0) && (vflash->nv_slot1 == NULL)) {
//         flash_area_open(FLASH_AREA_IMAGE_1, &vflash->nv_slot1);

//         /*
//          * If application side provided image check if same image was already
//          * provided before and if so mark virtual slot as empty
//          */
//         if (nrf54h20_rad_images_match()) {
//             /* Same hash for current and pending slot, act as slot 2 was empty */
//             vflash->nv_image_size = 0;
//         }
//     }
//     fa = vflash->nv_slot1;

//     /* Copy data from image */
//     if (address < vflash->nv_image_size) {
//         if (address + write_bytes > vflash->nv_image_size) {
//             write_bytes = vflash->nv_image_size - address;
//         }
//         memcpy(wp, vflash->nv_image_address + address, write_bytes);
//         address += write_bytes;
//         wp += write_bytes;
//         num_bytes -= write_bytes;
//     }
//     /* Copy data from image trailer */
//     if (num_bytes > 0 && fa != NULL && address >= (fa->fa_off + fa->fa_size - sizeof(swap_data))) {
//         swap_data = swap_data_template;
//         uint32_t off = address - (fa->fa_off + fa->fa_size - sizeof(swap_data));
//         write_bytes = num_bytes;
//         if (num_bytes > sizeof(swap_data) + off) {
//             write_bytes = sizeof(swap_data) - off;
//         }
//         memcpy(wp, ((uint8_t *)&swap_data) + off, write_bytes);
//         wp += write_bytes;
//         address += write_bytes;
//         num_bytes -= write_bytes;
//     }
//     /* Fill the rest with FF */
//     if (num_bytes > 0) {
//         memset(wp, 0xff, num_bytes);
//     }
//     return 0;
// }

// /*
//  * Flash write is done by writing 4 bytes at a time at a word boundary.
//  */
// static int
// nrf54h20_rad_vflash_write(const struct hal_flash *dev, uint32_t address,
//                          const void *src, uint32_t num_bytes)
// {
//     (void)dev;
//     (void)address;
//     (void)src;
//     (void)num_bytes;

//     return 0;
// }

// static int
// nrf54h20_rad_vflash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
// {
//     (void)dev;
//     (void)sector_address;

//     return 0;
// }

// static int
// nrf54h20_rad_vflash_sector_info(const struct hal_flash *dev, int idx,
//                                uint32_t *address, uint32_t *sz)
// {
//     (void)dev;

//     assert(idx < nrf54h20_flash_dev.hf_sector_cnt);
//     *address = idx * NRF5340_NET_VFLASH_SECTOR_SZ;
//     *sz = NRF5340_NET_VFLASH_SECTOR_SZ;

//     return 0;
// }

// static int
// nrf54h20_rad_vflash_init(const struct hal_flash *dev)
// {
//     struct nrf54h20_vflash *vflash = (struct nrf54h20_vflash *)dev;
//     const void *img_addr;
//     uint32_t image_size;

// #if MYNEWT_VAL(IPC_ICBMSG)
//     img_addr = 0;
//     image_size = 0;
// #else
//     img_addr = ipc_nrf54h20_rad_image_get(&image_size);
// #endif

//     /*
//      * Application side IPC will set ipc_share data
//      * (or GPMEM registers) to address and size of
//      * memory where net core image is present in application flash.
//      * If those values are 0, application image does not have embedded image,
//      * and there no need to provide any data.
//      * Set nv_image_size to 0 and all reads will return empty values (0xff)
//      */
//     if (img_addr) {
//         vflash->nv_image_address = img_addr;
//         vflash->nv_image_size = image_size;
//     }

//     return 0;
// }

// static const struct hal_flash_funcs nrf54h20_rad_vflash_funcs = {
//     .hff_read = nrf54h20_rad_vflash_read,
//     .hff_write = nrf54h20_rad_vflash_write,
//     .hff_erase_sector = nrf54h20_rad_vflash_erase_sector,
//     .hff_sector_info = nrf54h20_rad_vflash_sector_info,
//     .hff_init = nrf54h20_rad_vflash_init
// };

// struct nrf54h20_vflash nrf54h20_rad_vflash_dev = {
//     .nv_flash = {
//         .hf_itf = &nrf54h20_rad_vflash_funcs,
//         .hf_base_addr = 0x00000000,
//         .hf_size = 256 * 1024,
//         .hf_sector_cnt = 128,
//         .hf_align = 1,
//         .hf_erased_val = 0xff,
//     }
// };

// #endif
