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
//
//#include <string.h>
//#include <assert.h>
//#include <nrf.h>
//#include <mcu/nrf54h20_rad_hal.h>
//#include <hal/hal_flash_int.h>
//
//#define NRF5340_NET_FLASH_SECTOR_SZ 2048
//
//#define NRF5340_NET_FLASH_READY() (NRF_NVMC->READY == NVMC_READY_READY_Ready)
//
//static int
//nrf54h20_rad_flash_wait_ready(void)
//{
//    int i;
//
//    for (i = 0; i < 100000; i++) {
//        if (NRF_NVMC_NS->READY == NVMC_READY_READY_Ready) {
//            return 0;
//        }
//    }
//    return -1;
//}
//
//static int
//nrf54h20_rad_flash_read(const struct hal_flash *dev, uint32_t address, void *dst,
//                       uint32_t num_bytes)
//{
//    memcpy(dst, (void *)address, num_bytes);
//    return 0;
//}
//
///*
// * Flash write is done by writing 4 bytes at a time at a word boundary.
// */
//static int
//nrf54h20_rad_flash_write(const struct hal_flash *dev, uint32_t address,
//                        const void *src, uint32_t num_bytes)
//{
//    int sr;
//    int rc = -1;
//    uint32_t val;
//    int cnt;
//    uint32_t tmp;
//
//    if (nrf54h20_rad_flash_wait_ready()) {
//        return -1;
//    }
//    __HAL_DISABLE_INTERRUPTS(sr);
//    NRF_NVMC_NS->CONFIG = NVMC_CONFIG_WEN_Wen; /* Enable erase OP */
//    tmp = address & 0x3;
//    if (tmp) {
//        if (nrf54h20_rad_flash_wait_ready()) {
//            goto out;
//        }
//        /*
//         * Starts at a non-word boundary. Read 4 bytes which were there
//         * before, update with new data, and write back.
//         */
//        val = *(uint32_t *)(address & ~0x3);
//        cnt = 4 - tmp;
//        if (cnt > num_bytes) {
//            cnt = num_bytes;
//        }
//        memcpy((uint8_t *)&val + tmp, src, cnt);
//        *(uint32_t *)(address & ~0x3) = val;
//        address += cnt;
//        num_bytes -= cnt;
//        src += cnt;
//    }
//
//    while (num_bytes >= sizeof(uint32_t)) {
//        /*
//         * Write data 4 bytes at a time.
//         */
//        if (nrf54h20_rad_flash_wait_ready()) {
//            goto out;
//        }
//        *(uint32_t *)address = *(uint32_t *)src;
//        address += sizeof(uint32_t);
//        src += sizeof(uint32_t);
//        num_bytes -= sizeof(uint32_t);
//    }
//    if (num_bytes) {
//        /*
//         * Deal with the trailing bytes.
//         */
//        val = *(uint32_t *)address;
//        memcpy(&val, src, num_bytes);
//        if (nrf54h20_rad_flash_wait_ready()) {
//            goto out;
//        }
//        *(uint32_t *)address = val;
//    }
//
//    rc = nrf54h20_rad_flash_wait_ready();
//out:
//    NRF_NVMC_NS->CONFIG = NVMC_CONFIG_WEN_Ren;
//    __HAL_ENABLE_INTERRUPTS(sr);
//    return rc;
//}
//
//static int
//nrf54h20_rad_flash_erase_sector(const struct hal_flash *dev, uint32_t sector_address)
//{
//    int sr;
//    int rc;
//
//    sector_address &= ~(NRF5340_NET_FLASH_SECTOR_SZ - 1);
//
//    if (nrf54h20_rad_flash_wait_ready()) {
//        return -1;
//    }
//    __HAL_DISABLE_INTERRUPTS(sr);
//
//    NRF_NVMC_NS->CONFIG = NVMC_CONFIG_WEN_Een; /* Enable erase OP */
//    *(uint32_t *)sector_address = 0xFFFFFFFF;
//
//    rc = nrf54h20_rad_flash_wait_ready();
//
//    NRF_NVMC_NS->CONFIG = NVMC_CONFIG_WEN_Ren;
//    __HAL_ENABLE_INTERRUPTS(sr);
//
//    return rc;
//}
//
//static int
//nrf54h20_rad_flash_erase(const struct hal_flash *dev, uint32_t address,
//                        uint32_t num_bytes)
//{
//    uint32_t sector_address;
//
//    if (address + num_bytes < dev->hf_base_addr ||
//        address > dev->hf_base_addr + dev->hf_size) {
//        return -1;
//    }
//
//    sector_address = address & ~(NRF5340_NET_FLASH_SECTOR_SZ - 1);
//    num_bytes += address - sector_address;
//    num_bytes = (num_bytes + NRF5340_NET_FLASH_SECTOR_SZ - 1) & ~(NRF5340_NET_FLASH_SECTOR_SZ - 1);
//    if (sector_address < dev->hf_base_addr) {
//        num_bytes -= dev->hf_base_addr - sector_address;
//        sector_address = dev->hf_base_addr;
//    }
//
//    while (num_bytes > 0 && sector_address < dev->hf_base_addr + dev->hf_size) {
//        nrf54h20_rad_flash_erase_sector(dev, sector_address);
//        num_bytes -= NRF5340_NET_FLASH_SECTOR_SZ;
//        sector_address += NRF5340_NET_FLASH_SECTOR_SZ;
//    }
//
//    return 0;
//}
//
//static int
//nrf54h20_rad_flash_sector_info(const struct hal_flash *dev, int idx,
//                              uint32_t *address, uint32_t *sz)
//{
//    assert(idx < nrf54h20_flash_dev.hf_sector_cnt);
//    *address = dev->hf_base_addr + idx * NRF5340_NET_FLASH_SECTOR_SZ;
//    *sz = NRF5340_NET_FLASH_SECTOR_SZ;
//    return 0;
//}
//
//static int
//nrf54h20_rad_flash_init(const struct hal_flash *dev)
//{
//    return 0;
//}
//
//static const struct hal_flash_funcs nrf54h20_rad_flash_funcs = {
//    .hff_read = nrf54h20_rad_flash_read,
//    .hff_write = nrf54h20_rad_flash_write,
//    .hff_erase_sector = nrf54h20_rad_flash_erase_sector,
//    .hff_sector_info = nrf54h20_rad_flash_sector_info,
//    .hff_init = nrf54h20_rad_flash_init,
//    .hff_erase = nrf54h20_rad_flash_erase,
//};
//
//const struct hal_flash nrf54h20_flash_dev = {
//    .hf_itf = &nrf54h20_rad_flash_funcs,
//    .hf_base_addr = 0x01000000,
//    .hf_size = 256 * 1024, /* XXX read from factory info? */
//    .hf_sector_cnt = 128,   /* XXX read from factory info? */
//    .hf_align = 1,
//    .hf_erased_val = 0xff,
//};
