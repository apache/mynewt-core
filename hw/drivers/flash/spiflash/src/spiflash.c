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

#include <string.h>
#include <assert.h>

#include "os/mynewt.h"
#include <bsp/bsp.h>
#include <hal/hal_spi.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash.h>
#include <hal/hal_flash_int.h>
#include <spiflash/spiflash.h>
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include <bus/drivers/spi_common.h>
#endif

#if MYNEWT_VAL(SPIFLASH)

#if MYNEWT_VAL(SPIFLASH_SPI_CS_PIN) < 0
#error SPIFLASH_SPI_CS_PIN must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(SPIFLASH_SECTOR_COUNT) == 0
#error SPIFLASH_SECTOR_COUNT must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(SPIFLASH_SECTOR_SIZE) == 0
#error SPIFLASH_SECTOR_SIZE must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(SPIFLASH_PAGE_SIZE) == 0
#error SPIFLASH_PAGE_SIZE must be set to the correct value in bsp syscfg.yml
#endif

#if MYNEWT_VAL(SPIFLASH_BAUDRATE) == 0
#error SPIFLASH_BAUDRATE must be set to the correct value in bsp syscfg.yml
#endif

static void spiflash_release_power_down_macronix(struct spiflash_dev *dev) __attribute__((unused));
static void spiflash_release_power_down_generic(struct spiflash_dev *dev) __attribute__((unused));

#define STD_FLASH_CHIP(name, mfid, typ, cap, release_power_down) \
    { \
        .fc_jedec_id = { \
            .ji_manufacturer = mfid, \
            .ji_type = typ, \
            .ji_capacity = cap, \
        }, \
        .fc_release_power_down = release_power_down, \
    }


#define ISSI_CHIP(name, typ, cap) \
    STD_FLASH_CHIP(name, JEDEC_MFC_ISSI, typ, cap, spiflash_release_power_down_generic)
#define WINBOND_CHIP(name, typ, cap) \
    STD_FLASH_CHIP(name, JEDEC_MFC_WINBOND, typ, cap, spiflash_release_power_down_generic)
#define MACRONIX_CHIP(name, typ, cap) \
    STD_FLASH_CHIP(name, JEDEC_MFC_MACRONIX, typ, cap, spiflash_release_power_down_generic)
/* Macro for chips with no RPD command */
#define MACRONIX_CHIP1(name, typ, cap) \
    STD_FLASH_CHIP(name, JEDEC_MFC_MACRONIX, typ, cap, spiflash_release_power_down_macronix)
#define GIGADEVICE_CHIP(name, typ, cap) \
    STD_FLASH_CHIP(name, JEDEC_MFC_GIGADEVICE, typ, cap, spiflash_release_power_down_generic)
#define MICRON_CHIP(name, typ, cap) \
    STD_FLASH_CHIP(name, JEDEC_MFC_MICRON, typ, cap, spiflash_release_power_down_generic)
#define ADESTO_CHIP(name, typ, cap) \
    STD_FLASH_CHIP(name, JEDEC_MFC_ADESTO, typ, cap, spiflash_release_power_down_generic)
#define EON_CHIP(name, typ, cap) \
    STD_FLASH_CHIP(name, JEDEC_MFC_EON, typ, cap, spiflash_release_power_down_generic)

static struct spiflash_chip supported_chips[] = {
#if MYNEWT_VAL(SPIFLASH_MANUFACTURER) && MYNEWT_VAL(SPIFLASH_MEMORY_TYPE) && MYNEWT_VAL(SPIFLASH_MEMORY_CAPACITY)
    STD_FLASH_CHIP("", MYNEWT_VAL(SPIFLASH_MANUFACTURER),
        MYNEWT_VAL(SPIFLASH_MEMORY_TYPE), MYNEWT_VAL(SPIFLASH_MEMORY_CAPACITY),
        spiflash_release_power_down_generic),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25D05C)
    GIGADEVICE_CHIP(GD25D05C, 0x40, FLASH_CAPACITY_512KBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LD05C)
    GIGADEVICE_CHIP(GD25LD05C, 0x60, FLASH_CAPACITY_512KBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LE05C)
    GIGADEVICE_CHIP(GD25LE05C, 0x60, FLASH_CAPACITY_512KBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LH05C)
    GIGADEVICE_CHIP(GD25LH05C, 0x60, FLASH_CAPACITY_512KBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25VD05B)
    GIGADEVICE_CHIP(GD25VD05B, 0x40, FLASH_CAPACITY_512KBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25WD05C)
    GIGADEVICE_CHIP(GD25WD05C, 0x64, FLASH_CAPACITY_512KBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25D10C)
    GIGADEVICE_CHIP(GD25D10C, 0x40, FLASH_CAPACITY_1MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LD10C)
    GIGADEVICE_CHIP(GD25LD10C, 0x60, FLASH_CAPACITY_1MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LE10C)
    GIGADEVICE_CHIP(GD25LE10C, 0x60, FLASH_CAPACITY_1MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LH10C)
    GIGADEVICE_CHIP(GD25LH10C, 0x60, FLASH_CAPACITY_1MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25VD10B)
    GIGADEVICE_CHIP(GD25VD10B, 0x40, FLASH_CAPACITY_1MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25WD10C)
    GIGADEVICE_CHIP(GD25WD10C, 0x64, FLASH_CAPACITY_1MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LE20C)
    GIGADEVICE_CHIP(GD25LE20C, 0x60, FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LH20C)
    GIGADEVICE_CHIP(GD25LH20C, 0x60, FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25D20C)
    GIGADEVICE_CHIP(GD25D20C, 0x40, FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LD20C)
    GIGADEVICE_CHIP(GD25LD20C, 0x60, FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25VE20C)
    GIGADEVICE_CHIP(GD25VE20C, 0x42, FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25WD20C)
    GIGADEVICE_CHIP(GD25WD20C, 0x64, FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LE40C)
    GIGADEVICE_CHIP(GD25LE40C, 0x60, FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LH40C)
    GIGADEVICE_CHIP(GD25LH40C, 0x60, FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25D40C)
    GIGADEVICE_CHIP(GD25D40C, 0x40, FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LD40C)
    GIGADEVICE_CHIP(GD25LD40C, 0x60, FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25WD40C)
    GIGADEVICE_CHIP(GD25WD40C, 0x64, FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25VE40C)
    GIGADEVICE_CHIP(GD25VE40C, 0x42, FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25VE40B)
    GIGADEVICE_CHIP(GD25VE40B, 0x60, FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25D80C)
    GIGADEVICE_CHIP(GD25D80C, 0x40, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LD80C)
    GIGADEVICE_CHIP(GD25LD80C, 0x60, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LE80C)
    GIGADEVICE_CHIP(GD25LE80C, 0x60, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LH80B)
    GIGADEVICE_CHIP(GD25LH80B, 0x60, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LH80C)
    GIGADEVICE_CHIP(GD25LH80C, 0x60, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25WD80C)
    GIGADEVICE_CHIP(GD25WD80C, 0x64, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25Q80C)
    GIGADEVICE_CHIP(GD25Q80C, 0x40, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25B16C)
    GIGADEVICE_CHIP(GD25B16C, 0x40, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LE16C)
    GIGADEVICE_CHIP(GD25LE16C, 0x60, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25LH16C)
    GIGADEVICE_CHIP(GD25LH16C, 0x60, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25Q16C)
    GIGADEVICE_CHIP(GD25Q16C, 0x40, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_GD25VE16C)
    GIGADEVICE_CHIP(GD25VE16C, 0x42, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L512E)
    MACRONIX_CHIP(MX25L512E, 0x20, FLASH_CAPACITY_512KBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L5121E)
    MACRONIX_CHIP(MX25L5121E, 0x22, FLASH_CAPACITY_512KBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L1021E)
    MACRONIX_CHIP(MX25L1021E, 0x22, FLASH_CAPACITY_1MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25R512F)
    MACRONIX_CHIP1(MX25R512F, 0x28, FLASH_CAPACITY_512KBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U5121E)
    MACRONIX_CHIP(MX25U5121E, 0x25, 0x20 | FLASH_CAPACITY_512KBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U1001E)
    MACRONIX_CHIP(MX25U1001E, 0x25, 0x20 | FLASH_CAPACITY_1MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25V512E)
    MACRONIX_CHIP(MX25V512E, 0x20, FLASH_CAPACITY_512KBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25V512F)
    MACRONIX_CHIP1(MX25V512F, 0x23, FLASH_CAPACITY_512KBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L1006E)
    MACRONIX_CHIP(MX25L1006E, 0x20, FLASH_CAPACITY_1MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L1026E)
    MACRONIX_CHIP(MX25L1026E, 0x20, FLASH_CAPACITY_1MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25R1035F)
    MACRONIX_CHIP1(MX25R1035F, 0x28, FLASH_CAPACITY_1MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25V1006E)
    MACRONIX_CHIP(MX25V1006E, 0x20, FLASH_CAPACITY_1MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25V1006F)
    MACRONIX_CHIP(MX25V1006F, 0x20, FLASH_CAPACITY_1MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25V1035F)
    MACRONIX_CHIP1(MX25V1035F, 0x23, FLASH_CAPACITY_1MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L2006E)
    MACRONIX_CHIP(MX25L2006E, 0x20, FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L2026E)
    MACRONIX_CHIP(MX25L2026E, 0x20, FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25R2035F)
    MACRONIX_CHIP1(MX25R2035F, 0x28, FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U2032E)
    MACRONIX_CHIP(MX25U2032E, 0x25, 0x20 | FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U2033E)
    MACRONIX_CHIP(MX25U2033E, 0x25, 0x20 | FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U2035F)
    MACRONIX_CHIP1(MX25U2035F, 0x25, 0x20 | FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25V2006E)
    MACRONIX_CHIP(MX25V2006E, 0x20, FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25V2033F)
    MACRONIX_CHIP(MX25V2033F, 0x20, FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25V2035F)
    MACRONIX_CHIP1(MX25V2035F, 0x23, FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L4006E)
    MACRONIX_CHIP(MX25L4006E, 0x20, FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L4026E)
    MACRONIX_CHIP(MX25L4026E, 0x20, FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25R4035F)
    MACRONIX_CHIP1(MX25R4035F, 0x28, FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U4032E)
    MACRONIX_CHIP(MX25U4032E, 0x25, 0x20 | FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U4033E)
    MACRONIX_CHIP(MX25U4033E, 0x25, 0x20 | FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U4035)
    MACRONIX_CHIP(MX25U4035, 0x25, 0x20 | FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U4035F)
    MACRONIX_CHIP1(MX25U4035F, 0x25, 0x20 | FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25V4006E)
    MACRONIX_CHIP(MX25V4006E, 0x20, FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25V4035F)
    MACRONIX_CHIP1(MX25V4035F, 0x23, FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U8035)
    MACRONIX_CHIP(MX25U8035, 0x25, 0x20 | FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L8006E)
    MACRONIX_CHIP(MX25L8006E, 0x20, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L8008E)
    MACRONIX_CHIP(MX25L8008E, 0x20, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L8035E)
    MACRONIX_CHIP(MX25L8035E, 0x20, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L8036E)
    MACRONIX_CHIP(MX25L8036E, 0x20, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L8073E)
    MACRONIX_CHIP(MX25L8073E, 0x20, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25R8035F)
    MACRONIX_CHIP1(MX25R8035F, 0x28, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U8032E)
    MACRONIX_CHIP(MX25U8032E, 0x25, 0x20 | FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U8033E)
    MACRONIX_CHIP(MX25U8033E, 0x25, 0x20 | FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U8035E)
    MACRONIX_CHIP(MX25U8035E, 0x25, 0x20 | FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U8035F)
    MACRONIX_CHIP1(MX25U8035F, 0x25, 0x20 | FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25V8006E)
    MACRONIX_CHIP(MX25V8006E, 0x20, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25V8033F)
    MACRONIX_CHIP1(MX25V8033F, 0x23, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25V8035F)
    MACRONIX_CHIP1(MX25V8035F, 0x23, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L1606E)
    MACRONIX_CHIP(MX25L1606E, 0x20, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L1608E)
    MACRONIX_CHIP(MX25L1608E, 0x20, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L1633E)
    MACRONIX_CHIP(MX25L1633E, 0x24, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L1635E)
    MACRONIX_CHIP(MX25L1635E, 0x25, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L1636E)
    MACRONIX_CHIP(MX25L1636E, 0x25, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L1673E)
    MACRONIX_CHIP(MX25L1673E, 0x24, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25R1635F)
    MACRONIX_CHIP1(MX25R1635F, 0x28, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U1633F)
    MACRONIX_CHIP1(MX25U1633F, 0x25, 0x20 | FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U1635E)
    MACRONIX_CHIP(MX25U1635E, 0x25, 0x20 | FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U1635F)
    MACRONIX_CHIP(MX25U1635F, 0x25, 0x20 | FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25V1635F)
    MACRONIX_CHIP1(MX25V1635F, 0x23, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L3206E)
    MACRONIX_CHIP(MX25L3206E, 0x20, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L3208E)
    MACRONIX_CHIP(MX25L3208E, 0x20, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L3233F)
    MACRONIX_CHIP(MX25L3233F, 0x20, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L3235E)
    MACRONIX_CHIP(MX25L3235E, 0x20, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L3236F)
    MACRONIX_CHIP(MX25L3236F, 0x20, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L3239E)
    MACRONIX_CHIP(MX25L3239E, 0x25, 0x20 | FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L3273E)
    MACRONIX_CHIP(MX25L3273E, 0x20, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25L3273F)
    MACRONIX_CHIP(MX25L3273F, 0x20, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25R3235F)
    MACRONIX_CHIP1(MX25R3235F, 0x28, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U3235E)
    MACRONIX_CHIP(MX25U3235E, 0x25, 0x20 | FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U3235F)
    MACRONIX_CHIP(MX25U3235F, 0x25, 0x20 | FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_MX25U3273F)
    MACRONIX_CHIP1(MX25U3273F, 0x25, 0x20 | FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_IS25LP080D)
    ISSI_CHIP(IS25LP080D, 0x60, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_IS25WP080D)
    ISSI_CHIP(IS25WP080D, 0x70, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_IS25WP040D)
    ISSI_CHIP(IS25WP040D, 0x70, FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_IS25WP020D)
    ISSI_CHIP(IS25WP020D, 0x70, FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_IS25LQ040B)
    ISSI_CHIP(IS25LQ040B, 0x40, FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_IS25LQ020B)
    ISSI_CHIP(IS25LQ020B, 0x40, FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_IS25LQ010B)
    ISSI_CHIP(IS25LQ010B, 0x40, FLASH_CAPACITY_1MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_IS25LQ512B)
    ISSI_CHIP(IS25LQ512B, 0x40, FLASH_CAPACITY_512KBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_IS25WQ040)
    ISSI_CHIP(IS25WQ040, 0x12, 0x4 | FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_IS25WQ020)
    ISSI_CHIP(IS25WQ020, 0x11, 0x4 | FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_IS25LQ025B)
    ISSI_CHIP(IS25LQ025B, 0x40, FLASH_CAPACITY_256KBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_IS25LP016D)
    ISSI_CHIP(IS25LP016D, 0x60, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_IS25WP016D)
    ISSI_CHIP(IS25WP016D, 0x70, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_IS25LP032D)
    ISSI_CHIP(IS25LP032D, 0x60, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_IS25WP032D)
    ISSI_CHIP(IS25WP032D, 0x70, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25X05CL)
    WINBOND_CHIP(W25X05CL, 0x30, FLASH_CAPACITY_512KBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q10EW)
    WINBOND_CHIP(W25Q10EW, 0x60, FLASH_CAPACITY_1MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25X10CL)
    WINBOND_CHIP(W25X10CL, 0x30, FLASH_CAPACITY_1MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q20CL)
    WINBOND_CHIP(W25Q20CL, 0x40, FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q20EW)
    WINBOND_CHIP(W25Q20EW, 0x60, FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25X20CL)
    WINBOND_CHIP(W25X20CL, 0x30, FLASH_CAPACITY_2MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q40CL)
    WINBOND_CHIP(W25Q40CL, 0x40, FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q40EW)
    WINBOND_CHIP(W25Q40EW, 0x60, FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25X40CL)
    WINBOND_CHIP(W25X40CL, 0x30, FLASH_CAPACITY_4MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q80DV)
    WINBOND_CHIP(W25Q80DV, 0x40, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q80DL)
    WINBOND_CHIP(W25Q80DL, 0x40, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q80EW)
    WINBOND_CHIP(W25Q80EW, 0x60, FLASH_CAPACITY_8MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q16DV)
    WINBOND_CHIP(W25Q16DV, 0x40, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q16DW)
    WINBOND_CHIP(W25Q16DW, 0x60, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q16FW)
    WINBOND_CHIP(W25Q16FW, 0x60, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q16JL)
    WINBOND_CHIP(W25Q16JL, 0x40, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q16JV_DTR)
    WINBOND_CHIP(W25Q16JV_DTR, 0x70, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q16JV_IQ)
    WINBOND_CHIP(W25Q16JV_IQ, 0x40, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q16JV_IM)
    WINBOND_CHIP(W25Q16JV_IM, 0x70, FLASH_CAPACITY_16MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q32BV)
    WINBOND_CHIP(W25Q32BV, 0x40, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q32FV)
    WINBOND_CHIP(W25Q32FV, 0x40, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q32FW)
    WINBOND_CHIP(W25Q32FW, 0x60, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q32JV)
    WINBOND_CHIP(W25Q32JV, 0x70, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q32JV_IQ)
    WINBOND_CHIP(W25Q32JV_IQ, 0x40, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q32JW)
    WINBOND_CHIP(W25Q32JW, 0x80, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_W25Q32JW_IQ)
    WINBOND_CHIP(W25Q32JW_IQ, 0x60, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_AT25SF041)
    ADESTO_CHIP(AT25SF041, 0x84, 1),
#endif
#if MYNEWT_VAL(SPIFLASH_AT25SF081)
    ADESTO_CHIP(AT25SF081, 0x85, 1),
#endif
#if MYNEWT_VAL(SPIFLASH_AT25DF081A)
    ADESTO_CHIP(AT25DF081A, 0x45, 1),
#endif
#if MYNEWT_VAL(SPIFLASH_AT25DL081)
    ADESTO_CHIP(AT25DL081, 0x45, 2),
#endif
#if MYNEWT_VAL(SPIFLASH_AT25SF161)
    ADESTO_CHIP(AT25SF161, 0x86, 1),
#endif
#if MYNEWT_VAL(SPIFLASH_AT25DL161)
    ADESTO_CHIP(AT25DL161, 0x46, 3),
#endif
#if MYNEWT_VAL(SPIFLASH_AT25SL321)
    ADESTO_CHIP(AT25SL321, 0x42, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_AT25SF321)
    ADESTO_CHIP(AT25SF321, 0x87, 1),
#endif
#if MYNEWT_VAL(SPIFLASH_AT25DF321A)
    ADESTO_CHIP(AT25DF321A, 0x47, 1),
#endif
#if MYNEWT_VAL(SPIFLASH_AT25QL321)
    ADESTO_CHIP(AT25QL321, 0x42, FLASH_CAPACITY_32MBIT),
#endif
#if MYNEWT_VAL(SPIFLASH_EON2580B)
    EON_CHIP(EN80B, 0x30, FLASH_CAPACITY_8MBIT),
#endif

    { {0} },
};

static int hal_spiflash_read(const struct hal_flash *hal_flash_dev, uint32_t addr,
        void *buf, uint32_t len);
static int hal_spiflash_write(const struct hal_flash *hal_flash_dev, uint32_t addr,
        const void *buf, uint32_t len);
static int hal_spiflash_erase_sector(const struct hal_flash *hal_flash_dev,
        uint32_t sector_address);
static int hal_spiflash_sector_info(const struct hal_flash *hal_flash_dev, int idx,
        uint32_t *address, uint32_t *sz);
static int hal_spiflash_init(const struct hal_flash *dev);
static int hal_spiflash_erase(const struct hal_flash *hal_flash_dev,
        uint32_t address, uint32_t sz);

static const struct hal_flash_funcs spiflash_flash_funcs = {
    .hff_read         = hal_spiflash_read,
    .hff_write        = hal_spiflash_write,
    .hff_erase_sector = hal_spiflash_erase_sector,
    .hff_sector_info  = hal_spiflash_sector_info,
    .hff_init         = hal_spiflash_init,
    .hff_erase        = hal_spiflash_erase,
};

static const struct spiflash_characteristics spiflash_characteristics = {
    .tse = {
        .typical = MYNEWT_VAL(SPIFLASH_TSE_TYPICAL),
        .maximum = MYNEWT_VAL(SPIFLASH_TSE_MAXIMUM)
    },
    .tbe1 = {
        .typical = MYNEWT_VAL(SPIFLASH_TBE1_TYPICAL),
        .maximum = MYNEWT_VAL(SPIFLASH_TBE1_MAXIMUM)
    },
    .tbe2 = {
        .typical = MYNEWT_VAL(SPIFLASH_TBE2_TYPICAL),
        .maximum = MYNEWT_VAL(SPIFLASH_TBE2_MAXIMUM)
    },
    .tce = {
        .typical = MYNEWT_VAL(SPIFLASH_TCE_TYPICAL),
        .maximum = MYNEWT_VAL(SPIFLASH_TCE_MAXIMUM)
    },
    .tpp = {
        .typical = MYNEWT_VAL(SPIFLASH_TPP_TYPICAL),
        .maximum = MYNEWT_VAL(SPIFLASH_TPP_MAXIMUM)
    },
    .tbp1 = {
        .typical = MYNEWT_VAL(SPIFLASH_TBP1_TYPICAL),
        .maximum = MYNEWT_VAL(SPIFLASH_TBP1_MAXIMUM)
    },
};

struct spiflash_dev spiflash_dev = {
    /* struct hal_flash for compatibility */
    .hal = {
        .hf_itf        = &spiflash_flash_funcs,
        .hf_base_addr  = 0,
        .hf_size       = MYNEWT_VAL(SPIFLASH_SECTOR_COUNT) *
                         MYNEWT_VAL(SPIFLASH_SECTOR_SIZE),
        .hf_sector_cnt = MYNEWT_VAL(SPIFLASH_SECTOR_COUNT),
        .hf_align      = 1,
        .hf_erased_val = 0xff,
    },

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
    /* SPI settings */
    .spi_settings = {
        .data_order = HAL_SPI_MSB_FIRST,
        .data_mode  = HAL_SPI_MODE3,
        .baudrate   = MYNEWT_VAL(SPIFLASH_BAUDRATE),
        .word_size  = HAL_SPI_WORD_SIZE_8BIT,
    },
    .spi_num            = MYNEWT_VAL(SPIFLASH_SPI_NUM),
    .spi_cfg            = NULL,
    .ss_pin             = MYNEWT_VAL(SPIFLASH_SPI_CS_PIN),
#endif
    .sector_size        = MYNEWT_VAL(SPIFLASH_SECTOR_SIZE),
    .page_size          = MYNEWT_VAL(SPIFLASH_PAGE_SIZE),

    .supported_chips = supported_chips,
    .characteristics = &spiflash_characteristics,
    .flash_chip = NULL,
};

static inline void spiflash_lock_no_apd(struct spiflash_dev *dev)
{
#if MYNEWT_VAL(OS_SCHEDULING)
    os_mutex_pend(&dev->lock, OS_TIMEOUT_NEVER);
#endif
}

static inline void spiflash_unlock_no_apd(struct spiflash_dev *dev)
{
#if MYNEWT_VAL(OS_SCHEDULING)
    os_mutex_release(&dev->lock);
#endif
}

static inline void spiflash_lock(struct spiflash_dev *dev)
{
#if MYNEWT_VAL(OS_SCHEDULING)
    os_mutex_pend(&dev->lock, OS_TIMEOUT_NEVER);
#endif

#if MYNEWT_VAL(SPIFLASH_AUTO_POWER_DOWN)
    if (dev->pd_active) {
        spiflash_release_power_down(dev);
    } else {
#if MYNEWT_VAL(OS_SCHEDULING)
        if (os_mutex_get_level(&dev->lock) == 1) {
            os_callout_stop(&dev->apd_tmo_co);
        }
#endif
    }
#endif
}

static inline void spiflash_unlock(struct spiflash_dev *dev)
{
#if MYNEWT_VAL(OS_SCHEDULING)
#if MYNEWT_VAL(SPIFLASH_AUTO_POWER_DOWN)
    if (dev->apd_tmo && !dev->pd_active &&
        (os_mutex_get_level(&dev->lock) == 1)) {
        os_callout_reset(&dev->apd_tmo_co, dev->apd_tmo);
    }
#endif

    os_mutex_release(&dev->lock);
#endif
}

static inline void
spiflash_cs_activate(struct spiflash_dev *dev)
{
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bus_spi_node *node = (struct bus_spi_node *)&dev->dev;
    hal_gpio_write(node->pin_cs, 0);
#else
    hal_gpio_write(dev->ss_pin, 0);
#endif
}

static inline void
spiflash_cs_deactivate(struct spiflash_dev *dev)
{
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bus_spi_node *node = &dev->dev;
    hal_gpio_write(node->pin_cs, 1);
#else
    hal_gpio_write(dev->ss_pin, 1);
#endif
}

void
spiflash_power_down(struct spiflash_dev *dev)
{
    uint8_t cmd[1] = { SPIFLASH_DEEP_POWER_DOWN };

    spiflash_lock_no_apd(dev);

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    bus_node_simple_write((struct os_dev *)&dev->dev, cmd, sizeof(cmd));
#else
    spiflash_cs_activate(dev);

    hal_spi_txrx(dev->spi_num, cmd, cmd, sizeof cmd);

    spiflash_cs_deactivate(dev);
#endif

#if MYNEWT_VAL(SPIFLASH_AUTO_POWER_DOWN)
    dev->pd_active = true;
#endif
    dev->ready = false;

    spiflash_unlock_no_apd(dev);
}

/*
 * Some Macronix chips don't have standard release power down command 0xAB.
 * Instead they use CS pin alone to wake up from sleep.
 */
static void
spiflash_release_power_down_macronix(struct spiflash_dev *dev)
{
    spiflash_cs_activate(dev);

    os_cputime_delay_usecs(20);

    spiflash_cs_deactivate(dev);
}

static void
spiflash_release_power_down_generic(struct spiflash_dev *dev)
{
    uint8_t cmd[1] = { SPIFLASH_RELEASE_POWER_DOWN };

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    bus_node_simple_write((struct os_dev *)&dev->dev, cmd, sizeof(cmd));
#else
    spiflash_cs_activate(dev);

    hal_spi_txrx(dev->spi_num, cmd, cmd, sizeof cmd);

    spiflash_cs_deactivate(dev);
#endif
}

void
spiflash_release_power_down(struct spiflash_dev *dev)
{
    spiflash_lock_no_apd(dev);

    if (dev->flash_chip->fc_release_power_down) {
        dev->flash_chip->fc_release_power_down(dev);
    }

#if MYNEWT_VAL(SPIFLASH_AUTO_POWER_DOWN)
    dev->pd_active = false;
#endif

    spiflash_unlock_no_apd(dev);
}

int
spiflash_auto_power_down_set(struct spiflash_dev *dev, uint32_t timeout_ms)
{
#if MYNEWT_VAL(SPIFLASH_AUTO_POWER_DOWN) && MYNEWT_VAL(OS_SCHEDULING)
    /*
     * Lock with no auto power down since we do not want to wake up flash here,
     * it will be done if APD is disabled and power down is active. Then unlock
     * with APD to make sure it's applied if needed.
     */

    spiflash_lock_no_apd(dev);

    dev->apd_tmo = os_time_ms_to_ticks32(timeout_ms);

    if (!dev->apd_tmo && dev->pd_active) {
        spiflash_release_power_down_generic(dev);
    }

    spiflash_unlock(dev);

    return 0;
#else
    /* Not supported */
    return -1;
#endif
}

#if MYNEWT_VAL(SPIFLASH_AUTO_POWER_DOWN)
static void
spiflash_apd_tmo_func(struct os_event *ev)
{
    struct spiflash_dev *dev = ev->ev_arg;

    spiflash_lock_no_apd(dev);

    if (dev->apd_tmo && !dev->pd_active) {
        spiflash_power_down(dev);
    }

    spiflash_unlock_no_apd(dev);
}
#endif

uint8_t
spiflash_read_jedec_id(struct spiflash_dev *dev,
        uint8_t *manufacturer, uint8_t *memory_type, uint8_t *capacity)
{
    uint8_t cmd[4] = { SPIFLASH_READ_JEDEC_ID, 0, 0, 0 };

    spiflash_lock(dev);

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    bus_node_simple_write_read_transact((struct os_dev *)&dev->dev,
        cmd, 1, cmd + 1, 3);
#else
    spiflash_cs_activate(dev);

    hal_spi_txrx(dev->spi_num, cmd, cmd, sizeof cmd);

    spiflash_cs_deactivate(dev);
#endif

    if (manufacturer) {
        *manufacturer = cmd[1];
    }

    if (memory_type) {
        *memory_type = cmd[2];
    }

    if (capacity) {
        *capacity = cmd[3];
    }

    spiflash_unlock(dev);

    return 0;
}

uint8_t
spiflash_read_status(struct spiflash_dev *dev)
{
    uint8_t val;
    const uint8_t cmd = SPIFLASH_READ_STATUS_REGISTER;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    bus_node_simple_write_read_transact((struct os_dev *)&dev->dev,
        &cmd, 1, &val, 1);
#else
    spiflash_lock(dev);

    spiflash_cs_activate(dev);

    hal_spi_tx_val(dev->spi_num, cmd);
    val = hal_spi_tx_val(dev->spi_num, 0xFF);

    spiflash_cs_deactivate(dev);

    spiflash_unlock(dev);
#endif

    return val;
}

static void
spiflash_delay_us(uint32_t usecs)
{
#if MYNEWT_VAL(OS_SCHEDULING)
    uint32_t ticks = os_time_ms_to_ticks32(usecs / 1000);
    if (ticks > 1) {
        os_time_delay(ticks);
    } else {
        os_cputime_delay_usecs(usecs);
    }
#else
    os_cputime_delay_usecs(usecs);
#endif
}

bool
spiflash_device_ready(struct spiflash_dev *dev)
{
    dev->ready = !(spiflash_read_status(dev) & SPIFLASH_STATUS_BUSY);

    return dev->ready;
}

static int
spiflash_wait_ready_till(struct spiflash_dev *dev, uint32_t timeout_us,
    uint32_t step_us)
{
    int rc = -1;
    uint32_t limit;

    if (dev->ready) {
        return 0;
    }

    if (step_us < MYNEWT_VAL(SPIFLASH_READ_STATUS_INTERVAL)) {
        step_us = MYNEWT_VAL(SPIFLASH_READ_STATUS_INTERVAL);
    } else if (step_us > 1000000) {
        /* Read status once per second max */
        step_us = 1000000;
    }

    spiflash_lock(dev);

    limit = os_cputime_get32() + os_cputime_usecs_to_ticks(timeout_us);
    do {
        if (spiflash_device_ready(dev)) {
            rc = 0;
            break;
        }
        spiflash_delay_us(step_us);
    } while (CPUTIME_LT(os_cputime_get32(), limit));

    spiflash_unlock(dev);

    return rc;
}

int
spiflash_wait_ready(struct spiflash_dev *dev, uint32_t timeout_ms)
{
    /*
     * Timeout is in ms, check status register 100 times in this time.
     * If it would be shorter time than SPIFLASH_READ_STATUS_INTERVAL
     * number of timer status register is checked will be smaler.
     */
    return spiflash_wait_ready_till(dev, timeout_ms * 1000, timeout_ms * 10);
}

int
spiflash_write_enable(struct spiflash_dev *dev)
{
    uint8_t cmd = SPIFLASH_WRITE_ENABLE;
    spiflash_lock(dev);

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    bus_node_simple_write((struct os_dev *)&dev->dev, &cmd, 1);
#else
    spiflash_cs_activate(dev);

    hal_spi_tx_val(dev->spi_num, cmd);

    spiflash_cs_deactivate(dev);
#endif

    spiflash_unlock(dev);

    return 0;
}

static int
hal_spiflash_read(const struct hal_flash *hal_flash_dev, uint32_t addr, void *buf,
                  uint32_t len)
{
    int err = 0;
#if MYNEWT_VAL(SPIFLASH_CACHE_SIZE)
    uint32_t cached_size;
    uint8_t *user_buf;
    uint32_t left;
#endif
    uint8_t cmd[] = { SPIFLASH_READ,
        (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)(addr) };
    struct spiflash_dev *dev;

    dev = (struct spiflash_dev *)hal_flash_dev;

    spiflash_lock(dev);

    err = spiflash_wait_ready(dev, 100);
    if (!err) {
#if MYNEWT_VAL(SPIFLASH_CACHE_SIZE)
        if ((dev->cached_addr <= addr) &&
            (addr < dev->cached_addr + MYNEWT_VAL(SPIFLASH_CACHE_SIZE))) {
            /* Check if everything is cached */
            if (len < MYNEWT_VAL(SPIFLASH_CACHE_SIZE) -
                      (addr - dev->cached_addr)) {
                /* Everything  was cached */
                memcpy(buf, dev->cache + addr - dev->cached_addr, len);
                len = 0;
            } else {
                /* Some data was cached */
                cached_size = MYNEWT_VAL(SPIFLASH_CACHE_SIZE) -
                    (addr - dev->cached_addr);
                memcpy(buf, dev->cache + addr - dev->cached_addr, cached_size);
                len -= cached_size;
                buf = (uint8_t *)buf + cached_size;
                addr += cached_size;
                cmd[1] = (uint8_t)(addr >> 16);
                cmd[2] = (uint8_t)(addr >> 8);
                cmd[3] = (uint8_t)(addr);
            }
        }
        left = len;
        user_buf = buf;
        /*
         * In case small amount of data was requested use cache buffer for
         * reading, instead of caller buffer that would be to small.
         */
        if (len > 0 && len < MYNEWT_VAL(SPIFLASH_CACHE_SIZE)) {
            len = MYNEWT_VAL(SPIFLASH_CACHE_SIZE);
            buf = dev->cache;
        }
#endif
        if (len > 0) {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
            bus_node_simple_write_read_transact((struct os_dev *)&dev->dev,
                &cmd, 4, buf, len);
#else
            spiflash_cs_activate(dev);

            /* Send command + address */
            hal_spi_txrx(dev->spi_num, cmd, NULL, sizeof cmd);
            /* For security mostly, do not output random data, fill it with FF */
            memset(buf, 0xFF, len);
            /* Tx buf does not matter, for simplicity pass read buffer */
            hal_spi_txrx(dev->spi_num, buf, buf, len);

            spiflash_cs_deactivate(dev);
#endif
#if MYNEWT_VAL(SPIFLASH_CACHE_SIZE)
            /* If buffer was too small, cache was used for reading */
            if (left < MYNEWT_VAL(SPIFLASH_CACHE_SIZE)) {
                memcpy(user_buf, dev->cache, left);
                dev->cached_addr = addr;
            } else {
                /* Read was done directly to user buffer, copy end to cache */
                dev->cached_addr = addr + left - MYNEWT_VAL(SPIFLASH_CACHE_SIZE);
                memcpy(dev->cache, (uint8_t *)buf + left -
                       MYNEWT_VAL(SPIFLASH_CACHE_SIZE),
                       MYNEWT_VAL(SPIFLASH_CACHE_SIZE));
            }
#endif
        }
    }

    spiflash_unlock(dev);

    return 0;
}

static int
hal_spiflash_write(const struct hal_flash *hal_flash_dev, uint32_t addr,
        const void *buf, uint32_t len)
{
    uint8_t cmd[4] = { SPIFLASH_PAGE_PROGRAM };
    const uint8_t *u8buf = buf;
    struct spiflash_dev *dev = (struct spiflash_dev *)hal_flash_dev;
    uint32_t page_limit;
    uint32_t to_write;
    uint32_t pp_time_typical;
    uint32_t pp_time_maximum;
    int rc = 0;

    u8buf = (uint8_t *)buf;

    spiflash_lock(dev);

    if (spiflash_wait_ready(dev, 100) != 0) {
        rc = -1;
        goto err;
    }

#if MYNEWT_VAL(SPIFLASH_CACHE_SIZE)
    dev->cached_addr = 0xFFFFFFFF;
#endif

    pp_time_typical = dev->characteristics->tbp1.typical;
    pp_time_maximum = dev->characteristics->tpp.maximum;
    if (pp_time_maximum < pp_time_typical) {
        pp_time_maximum = pp_time_typical;
    }

    while (len) {
        spiflash_write_enable(dev);

        cmd[1] = (uint8_t)(addr >> 16);
        cmd[2] = (uint8_t)(addr >> 8);
        cmd[3] = (uint8_t)(addr);

        page_limit = (addr & ~(dev->page_size - 1)) + dev->page_size;
        to_write = page_limit - addr > len ? len :  page_limit - addr;

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
        bus_node_lock((struct os_dev *)&dev->dev,
            BUS_NODE_LOCK_DEFAULT_TIMEOUT);
        bus_node_write((struct os_dev *)&dev->dev,
            cmd, 4, BUS_NODE_LOCK_DEFAULT_TIMEOUT, BUS_F_NOSTOP);
        bus_node_simple_write((struct os_dev *)&dev->dev, u8buf, to_write);
        bus_node_unlock((struct os_dev *)&dev->dev);
#else
        spiflash_cs_activate(dev);
        hal_spi_txrx(dev->spi_num, cmd, NULL, sizeof cmd);
        hal_spi_txrx(dev->spi_num, (void *)u8buf, NULL, to_write);
        spiflash_cs_deactivate(dev);
#endif
        /* Now we know that device is not ready */
        dev->ready = false;
        spiflash_delay_us(pp_time_typical);
        rc = spiflash_wait_ready_till(dev, pp_time_maximum - pp_time_typical,
            (pp_time_maximum - pp_time_typical) / 10);
        if (rc) {
            break;
        }
        addr += to_write;
        u8buf += to_write;
        len -= to_write;

    }
err:
    spiflash_unlock(dev);

    return rc;
}

static int
hal_spiflash_erase_sector(const struct hal_flash *hal_flash_dev, uint32_t addr)
{
    struct spiflash_dev *dev = (struct spiflash_dev *)hal_flash_dev;

    return spiflash_sector_erase(dev, addr);
}

static int
hal_spiflash_erase(const struct hal_flash *hal_flash_dev,
    uint32_t address, uint32_t size)
{
    struct spiflash_dev *dev = (struct spiflash_dev *)hal_flash_dev;

    return spiflash_erase(dev, address, size);
}

static int
spiflash_execute_erase(struct spiflash_dev *dev, const uint8_t *buf,
                       uint32_t size,
                       const struct spiflash_time_spec *delay_spec)
{
    int rc = 0;
    uint32_t wait_time_us;
    uint32_t start_time;

    spiflash_lock(dev);

#if MYNEWT_VAL(SPIFLASH_CACHE_SIZE)
    dev->cached_addr = 0xFFFFFFFF;
#endif

    if (spiflash_wait_ready(dev, 100) != 0) {
        rc = -1;
        goto err;
    }

    spiflash_write_enable(dev);

    spiflash_read_status(dev);

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    bus_node_simple_write((struct os_dev *)&dev->dev, buf, (uint16_t)size);
#else
    spiflash_cs_activate(dev);

    hal_spi_txrx(dev->spi_num, (void *)buf, NULL, size);

    spiflash_cs_deactivate(dev);
#endif
    /* Now we know that device is not ready */
    dev->ready = false;

    start_time = os_cputime_get32();
    /* Wait typical erase time before starting polling for ready */
    spiflash_delay_us(delay_spec->typical);

    wait_time_us = os_cputime_ticks_to_usecs(os_cputime_get32() - start_time);
    if (wait_time_us > delay_spec->maximum) {
        wait_time_us = 0;
    } else {
        wait_time_us = delay_spec->maximum - wait_time_us;
    }

    /* Poll status ready for remaining time */
    rc = spiflash_wait_ready_till(dev, wait_time_us, wait_time_us / 50);
err:
    spiflash_unlock(dev);

    return rc;
}

static int
hal_spiflash_sector_info(const struct hal_flash *hal_flash_dev, int idx,
        uint32_t *address, uint32_t *sz)
{
    const struct spiflash_dev *dev = (const struct spiflash_dev *)hal_flash_dev;

    *address = idx * dev->sector_size;
    *sz = dev->sector_size;
    return 0;
}

static int
spiflash_erase_cmd(struct spiflash_dev *dev, uint8_t cmd, uint32_t addr,
                   const struct spiflash_time_spec *time_spec)
{
    uint8_t buf[4] = { cmd, (uint8_t)(addr >> 16U), (uint8_t)(addr >> 8U),
                       (uint8_t)addr };
    return spiflash_execute_erase(dev, buf, sizeof(buf), time_spec);

}

int
spiflash_sector_erase(struct spiflash_dev *dev, uint32_t addr)
{
    return spiflash_erase_cmd(dev, SPIFLASH_SECTOR_ERASE, addr,
                              &dev->characteristics->tse);
}

#if MYNEWT_VAL(SPIFLASH_BLOCK_ERASE_32BK)
int
spiflash_block_32k_erase(struct spiflash_dev *dev, uint32_t addr)
{
    return spiflash_erase_cmd(dev, SPIFLASH_BLOCK_ERASE_32KB, addr,
                              &dev->characteristics->tbe1);
}
#endif

#if MYNEWT_VAL(SPIFLASH_BLOCK_ERASE_64BK)
int
spiflash_block_64k_erase(struct spiflash_dev *dev, uint32_t addr)
{
    return spiflash_erase_cmd(dev, SPIFLASH_BLOCK_ERASE_64KB, addr,
                              &dev->characteristics->tbe2);
}
#endif

int
spiflash_chip_erase(struct spiflash_dev *dev)
{
    uint8_t buf[1] = { SPIFLASH_CHIP_ERASE };

    return spiflash_execute_erase(dev, buf, sizeof(buf),
                                  &dev->characteristics->tce);
}

int
spiflash_erase(struct spiflash_dev *dev, uint32_t address, uint32_t size)
{
    int rc = 0;

    if (address == 0 && size == dev->hal.hf_size) {
        return spiflash_chip_erase(dev);
    }
    address &= ~0xFFFU;
    while (size) {
#if MYNEWT_VAL(SPIFLASH_BLOCK_ERASE_64BK)
        if ((address & 0xFFFFU) == 0 && (size >= 0x10000)) {
            /* 64 KB erase if possible */
            rc = spiflash_block_64k_erase(dev, address);
            if (rc) {
                goto err;
            }
            address += 0x10000;
            size -= 0x10000;
            continue;
        }
#endif
#if MYNEWT_VAL(SPIFLASH_BLOCK_ERASE_32BK)
        if ((address & 0x7FFFU) == 0 && (size >= 0x8000)) {
            /* 32 KB erase if possible */
            rc = spiflash_block_32k_erase(dev, address);
            if (rc) {
                goto err;
            }
            address += 0x8000;
            size -= 0x8000;
            continue;
        }
#endif
        rc = spiflash_sector_erase(dev, address);
        if (rc) {
            goto err;
        }
        address += MYNEWT_VAL(SPIFLASH_SECTOR_SIZE);
        if (size > MYNEWT_VAL(SPIFLASH_SECTOR_SIZE)) {
            size -= MYNEWT_VAL(SPIFLASH_SECTOR_SIZE);
        } else {
            size = 0;
        }
    }
err:
    return rc;
}

int
spiflash_identify(struct spiflash_dev *dev)
{
    int i;
    int j;
    uint8_t manufacturer = 0;
    uint8_t memory_type = 0;
    uint8_t capacity = 0;
    int release_power_down_count = 0;
    /* Table with unique release power down functions */
    void (*rpd[3])(struct spiflash_dev *);
    int rc = 0;

    /* List of supported spi flash chips can be found in:
     * hw/drivers/flash/spiflash/chips/sysconfig.yml
     */
    static_assert((sizeof(supported_chips) / sizeof(supported_chips[0])) > 1,
        "At lease one spiflash chip must be specified in sysconfig with SPIFLASH_<chipid>:1");

    spiflash_lock(dev);

    /* Only one chip specified, no need for search*/
    if ((sizeof(supported_chips) / sizeof(supported_chips[0])) == 2) {
        supported_chips[0].fc_release_power_down(dev);
        spiflash_read_jedec_id(dev, &manufacturer, &memory_type, &capacity);
        /* If BSP defined SpiFlash manufacturer or memory type does not
         * match SpiFlash is most likely not connected, connected to
         * different pins, or of different type.
         * It is unlikely that flash depended packaged will work correctly.
         */
        assert(manufacturer == supported_chips[0].fc_jedec_id.ji_manufacturer &&
               memory_type == supported_chips[0].fc_jedec_id.ji_type &&
               capacity == supported_chips[0].fc_jedec_id.ji_capacity);
        if (manufacturer != supported_chips[0].fc_jedec_id.ji_manufacturer ||
            memory_type != supported_chips[0].fc_jedec_id.ji_type ||
            capacity != supported_chips[0].fc_jedec_id.ji_capacity) {
            rc = -1;
            goto err;
        }
        dev->flash_chip = &supported_chips[0];
    } else {
        for (i = 0; supported_chips[i].fc_jedec_id.ji_manufacturer != 0; ++i) {
            for (j = 0; j < release_power_down_count; ++j) {
                if (rpd[j] == supported_chips[i].fc_release_power_down) {
                    /* release power down function same as already execute one
                     * no need check more.
                     */
                    break;
                }
            }
            /* New function found, add to function table and call */
            if (j == release_power_down_count) {
                rpd[j] = supported_chips[i].fc_release_power_down;
                rpd[j](dev);
                spiflash_read_jedec_id(dev, &manufacturer, &memory_type, &capacity);
                if ((manufacturer == 0xFF && memory_type == 0xFF && capacity == 0xFF) ||
                    (manufacturer == 0 && memory_type == 0 && capacity == 0)) {
                    /* Most likely release did not work or device is not
                     * correctly configured (wrong pins).
                     * Try another release power down method if found.
                     */
                    continue;
                }
                /* Something was read from flash, do not try another power up
                 * just check if chip should be supported */
                break;
            }
        }
        for (i = 0; supported_chips[i].fc_jedec_id.ji_manufacturer != 0; ++i) {
            if (manufacturer ==  supported_chips[i].fc_jedec_id.ji_manufacturer &&
                memory_type == supported_chips[i].fc_jedec_id.ji_type &&
                capacity == supported_chips[i].fc_jedec_id.ji_capacity) {
                /* Device is supported */
                dev->flash_chip = &supported_chips[i];
                break;
            }
        }
        if (dev->flash_chip == NULL) {
            /* Not supported chip */
            assert(0);
            rc = -1;
        }
    }
err:
    spiflash_unlock(dev);

    return rc;
}

static int
hal_spiflash_init(const struct hal_flash *hal_flash_dev)
{
    int rc;
    struct spiflash_dev *dev;

    dev = (struct spiflash_dev *)hal_flash_dev;

#if MYNEWT_VAL(SPIFLASH_AUTO_POWER_DOWN)
    os_mutex_init(&dev->lock);
    os_callout_init(&dev->apd_tmo_co, os_eventq_dflt_get(),
                    spiflash_apd_tmo_func, dev);
#endif

#if !MYNEWT_VAL(BUS_DRIVER_PRESENT)
    hal_gpio_init_out(dev->ss_pin, 1);

    (void)hal_spi_disable(dev->spi_num);

    rc = hal_spi_config(dev->spi_num, &dev->spi_settings);
    if (rc) {
        return (rc);
    }

    hal_spi_set_txrx_cb(dev->spi_num, NULL, NULL);
    hal_spi_enable(dev->spi_num);
#endif
    rc = spiflash_identify(dev);

    return rc;
}

#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
static void
init_node_cb(struct bus_node *bnode, void *arg)
{
    /* TODO: Check if something else should be done here */
}

int
spiflash_create_spi_dev(struct bus_spi_node *node, const char *name,
                        const struct bus_spi_node_cfg *spi_cfg)
{
    struct bus_node_callbacks cbs = {
        .init = init_node_cb,
    };
    int rc;

    bus_node_set_callbacks((struct os_dev *)node, &cbs);

    rc = bus_spi_node_create(name, node, spi_cfg, NULL);

    return rc;
}
#endif

#endif

