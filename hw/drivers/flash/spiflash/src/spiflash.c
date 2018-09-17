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

#include "os/mynewt.h"
#include <hal/hal_spi.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash.h>
#include <hal/hal_flash_int.h>
#include <spiflash/spiflash.h>

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

void spiflash_release_power_down_macronix(struct spiflash_dev *dev);
void spiflash_release_power_down(struct spiflash_dev *dev);

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
    STD_FLASH_CHIP(name, JEDEC_MFC_ISSI, typ, cap, spiflash_release_power_down)
#define WINBOND_CHIP(name, typ, cap) \
    STD_FLASH_CHIP(name, JEDEC_MFC_WINBOND, typ, cap, spiflash_release_power_down)
#define MACRONIX_CHIP(name, typ, cap) \
    STD_FLASH_CHIP(name, JEDEC_MFC_MACRONIX, typ, cap, spiflash_release_power_down)
/* Macro for chips with no RPD command */
#define MACRONIX_CHIP1(name, typ, cap) \
    STD_FLASH_CHIP(name, JEDEC_MFC_MACRONIX, typ, cap, spiflash_release_power_down_macronix)
#define GIGADEVICE_CHIP(name, typ, cap) \
    STD_FLASH_CHIP(name, JEDEC_MFC_GIGADEVICE, typ, cap, spiflash_release_power_down)
#define MICRON_CHIP(name, typ, cap) \
    STD_FLASH_CHIP(name, JEDEC_MFC_MICRON, typ, cap, spiflash_release_power_down)
#define ADESTO_CHIP(name, typ, cap) \
    STD_FLASH_CHIP(name, JEDEC_MFC_ADESTO, typ, cap, spiflash_release_power_down)

static struct spiflash_chip supported_chips[] = {
#if MYNEWT_VAL(SPIFLASH_MANUFACTURER) && MYNEWT_VAL(SPIFLASH_MEMORY_TYPE) && MYNEWT_VAL(SPIFLASH_MEMORY_CAPACITY)
    STD_FLASH_CHIP("", MYNEWT_VAL(SPIFLASH_MANUFACTURER),
        MYNEWT_VAL(SPIFLASH_MEMORY_TYPE), MYNEWT_VAL(SPIFLASH_MEMORY_CAPACITY),
        spiflash_release_power_down),
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

    { {0} },
};

static int spiflash_read(const struct hal_flash *hal_flash_dev, uint32_t addr,
        void *buf, uint32_t len);
static int spiflash_write(const struct hal_flash *hal_flash_dev, uint32_t addr,
        const void *buf, uint32_t len);
static int spiflash_erase_sector(const struct hal_flash *hal_flash_dev,
        uint32_t sector_address);
static int spiflash_sector_info(const struct hal_flash *hal_flash_dev, int idx,
        uint32_t *address, uint32_t *sz);

static const struct hal_flash_funcs spiflash_flash_funcs = {
    .hff_read         = spiflash_read,
    .hff_write        = spiflash_write,
    .hff_erase_sector = spiflash_erase_sector,
    .hff_sector_info  = spiflash_sector_info,
    .hff_init         = spiflash_init,
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

    /* SPI settings */
    .spi_settings = {
        .data_order = HAL_SPI_MSB_FIRST,
        .data_mode  = HAL_SPI_MODE3,
        .baudrate   = MYNEWT_VAL(SPIFLASH_BAUDRATE),
        .word_size  = HAL_SPI_WORD_SIZE_8BIT,
    },

    .sector_size        = MYNEWT_VAL(SPIFLASH_SECTOR_SIZE),
    .page_size          = MYNEWT_VAL(SPIFLASH_PAGE_SIZE),
    .spi_num            = MYNEWT_VAL(SPIFLASH_SPI_NUM),
    .spi_cfg            = NULL,
    .ss_pin             = MYNEWT_VAL(SPIFLASH_SPI_CS_PIN),

    .supported_chips = supported_chips,
    .flash_chip = NULL,
};

static inline void
spiflash_cs_activate(struct spiflash_dev *dev)
{
    hal_gpio_write(dev->ss_pin, 0);
}

static inline void
spiflash_cs_deactivate(struct spiflash_dev *dev)
{
    hal_gpio_write(dev->ss_pin, 1);
}

void
spiflash_power_down(struct spiflash_dev *dev)
{
    uint8_t cmd[1] = { SPIFLASH_DEEP_POWER_DOWN };

    spiflash_cs_activate(dev);

    hal_spi_txrx(dev->spi_num, cmd, cmd, sizeof cmd);

    spiflash_cs_deactivate(dev);
}

/*
 * Some Macronix chips don't have standard release power down command 0xAB.
 * Instead they use CS pin alone to wake up from sleep.
 */
void
spiflash_release_power_down_macronix(struct spiflash_dev *dev)
{
    spiflash_cs_activate(dev);

    os_cputime_delay_usecs(20);

    spiflash_cs_deactivate(dev);
}

void
spiflash_release_power_down(struct spiflash_dev *dev)
{
    uint8_t cmd[1] = { SPIFLASH_RELEASE_POWER_DOWN };

    spiflash_cs_activate(dev);

    hal_spi_txrx(dev->spi_num, cmd, cmd, sizeof cmd);

    spiflash_cs_deactivate(dev);
}

uint8_t
spiflash_read_jedec_id(struct spiflash_dev *dev,
        uint8_t *manufacturer, uint8_t *memory_type, uint8_t *capacity)
{
    uint8_t cmd[4] = { SPIFLASH_READ_JEDEC_ID, 0, 0, 0 };

    spiflash_cs_activate(dev);

    hal_spi_txrx(dev->spi_num, cmd, cmd, sizeof cmd);

    spiflash_cs_deactivate(dev);

    if (manufacturer) {
        *manufacturer = cmd[1];
    }

    if (memory_type) {
        *memory_type = cmd[2];
    }

    if (capacity) {
        *capacity = cmd[3];
    }

    return 0;
}

uint8_t
spiflash_read_status(struct spiflash_dev *dev)
{
    uint8_t val;

    spiflash_cs_activate(dev);

    hal_spi_tx_val(dev->spi_num, SPIFLASH_READ_STATUS_REGISTER);
    val = hal_spi_tx_val(dev->spi_num, 0xFF);

    spiflash_cs_deactivate(dev);

    return val;
}

bool
spiflash_device_ready(struct spiflash_dev *dev)
{
    return !(spiflash_read_status(dev) & SPIFLASH_STATUS_BUSY);
}

int
spiflash_wait_ready(struct spiflash_dev *dev, uint32_t timeout_ms)
{
    uint32_t ticks;
    os_time_t exp_time;
    os_time_ms_to_ticks(timeout_ms, &ticks);
    exp_time = os_time_get() + ticks;

    while (!spiflash_device_ready(dev)) {
        if (os_time_get() > exp_time) {
            return -1;
        }
    }
    return 0;
}

int
spiflash_write_enable(struct spiflash_dev *dev)
{
    spiflash_cs_activate(dev);

    hal_spi_tx_val(dev->spi_num, SPIFLASH_WRITE_ENABLE);

    spiflash_cs_deactivate(dev);

    return 0;
}

int
spiflash_read(const struct hal_flash *hal_flash_dev, uint32_t addr, void *buf,
        uint32_t len)
{
    int err = 0;
    uint8_t cmd[] = { SPIFLASH_READ,
        (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)(addr) };
    struct spiflash_dev *dev;

    dev = (struct spiflash_dev *)hal_flash_dev;

    err = spiflash_wait_ready(dev, 100);
    if (!err) {
        spiflash_cs_activate(dev);

        /* Send command + address */
        hal_spi_txrx(dev->spi_num, cmd, NULL, sizeof cmd);
        /* For security mostly, do not output random data, fill it with FF */
        memset(buf, 0xFF, len);
        /* Tx buf does not matter, for simplicity pass read buffer */
        hal_spi_txrx(dev->spi_num, buf, buf, len);

        spiflash_cs_deactivate(dev);
    }

    return 0;
}

int
spiflash_write(const struct hal_flash *hal_flash_dev, uint32_t addr,
        const void *buf, uint32_t len)
{
    uint8_t cmd[4] = { SPIFLASH_PAGE_PROGRAM };
    const uint8_t *u8buf = buf;
    struct spiflash_dev *dev = (struct spiflash_dev *)hal_flash_dev;
    uint32_t page_limit;
    uint32_t to_write;

    u8buf = (uint8_t *)buf;

    while (len) {
        if (spiflash_wait_ready(dev, 100) != 0) {
            return -1;
        }

        spiflash_write_enable(dev);

        cmd[1] = (uint8_t)(addr >> 16);
        cmd[2] = (uint8_t)(addr >> 8);
        cmd[3] = (uint8_t)(addr);

        page_limit = (addr & ~(dev->page_size - 1)) + dev->page_size;
        to_write = page_limit - addr > len ? len :  page_limit - addr;

        spiflash_cs_activate(dev);
        hal_spi_txrx(dev->spi_num, cmd, NULL, sizeof cmd);
        hal_spi_txrx(dev->spi_num, (void *)u8buf, NULL, to_write);
        spiflash_cs_deactivate(dev);

        addr += to_write;
        u8buf += to_write;
        len -= to_write;

        spiflash_wait_ready(dev, 100);
    }

    return 0;
}

int
spiflash_erase_sector(const struct hal_flash *hal_flash_dev,
        uint32_t addr)
{
    struct spiflash_dev *dev;
    uint8_t cmd[4] = { SPIFLASH_SECTOR_ERASE,
        (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr };

    dev = (struct spiflash_dev *)hal_flash_dev;

    if (spiflash_wait_ready(dev, 100) != 0) {
        return -1;
    }

    spiflash_write_enable(dev);

    spiflash_read_status(dev);

    spiflash_cs_activate(dev);

    hal_spi_txrx(dev->spi_num, cmd, NULL, sizeof cmd);

    spiflash_cs_deactivate(dev);

    spiflash_wait_ready(dev, 100);

    return 0;
}

int
spiflash_sector_info(const struct hal_flash *hal_flash_dev, int idx,
        uint32_t *address, uint32_t *sz)
{
    const struct spiflash_dev *dev = (const struct spiflash_dev *)hal_flash_dev;

    *address = idx * dev->sector_size;
    *sz = dev->sector_size;
    return 0;
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

    /* List of supported spi flash chips can be found in:
     * hw/drivers/flash/spiflash/chips/sysconfig.yml
     */
    _Static_assert((sizeof(supported_chips) / sizeof(supported_chips[0])) > 1,
        "At lease one spiflash chip must be specified in sysconfig with SPIFLASH_<chipid>:1");

    /* Only one chip specified, no need for search*/
    if ((sizeof(supported_chips) / sizeof(supported_chips[0])) == 2) {
        spiflash_release_power_down(dev);
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
            return -1;
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
            return -1;
        }
    }
    return 0;
}

int
spiflash_init(const struct hal_flash *hal_flash_dev)
{
    int rc;
    struct spiflash_dev *dev;

    dev = (struct spiflash_dev *)hal_flash_dev;

    hal_gpio_init_out(dev->ss_pin, 1);

    rc = hal_spi_config(dev->spi_num, &dev->spi_settings);
    if (rc) {
        return (rc);
    }

    hal_spi_set_txrx_cb(dev->spi_num, NULL, NULL);
    hal_spi_enable(dev->spi_num);

    rc = spiflash_identify(dev);

    return rc;
}

#endif

