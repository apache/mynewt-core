<!--
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#  KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
-->

# PCA10059: NRF52840 USB dongle

## Flashing application without debugger

Board comes from factory with Nordic DFU Bootloader that does not recognize images created by **newt** tool.

Board with Nordic bootloader can be programmed using Nordic's [**nrfutil**](https://www.nordicsemi.com/Products/Development-tools/nRF-Util).

Here are steps to flash mynewt application:

1. Flash map of the application must be changed to match what Nordic bootloader expects.
Here is example target file with flash map suitable for Nordic bootloader.
target file: *targets/nordic_pca10059-blehci_nrf52dfu/target.yml*
```yml
target.app: "@apache-mynewt-nimble/apps/blehci"
target.bsp: "@apache-mynewt-core/hw/bsp/nordic_pca10059"
target.build_profile: debug

bsp.flash_map:
    areas:
        # NRF52 MBR area.
        FLASH_AREA_MBR:
            user_id: 20
            device: 0
            offset: 0x00000000
            size: 4kB
        # NRF52 bootloader ara.
        FLASH_AREA_NRF52_BOOTLOADER:
            user_id: 21
            device: 0
            offset: 0x000E0000
            size: 128kB
        # mynewt image
        FLASH_AREA_IMAGE_0:
            device: 0
            offset: 0x00001000
            size: 396kB

        # User areas.
        FLASH_AREA_REBOOT_LOG:
            user_id: 0
            device: 0
            offset: 0x000C8000
            size: 16kB
        FLASH_AREA_NFFS:
            user_id: 1
            device: 0
            offset: 0x000CC000
            size: 16kB
```
Example configuration file *targets/nordic_pca10059-blehci_nrf52dfu/syscfg.yml*:
```yml
syscfg.vals:
    # Skip includsion of image header in the build
    INCLUDE_IMAGE_HEADER: 0

    # Configuration for dongle to show up as bluetooth device in host system 
    BLE_TRANSPORT_HS: usb
    USBD_PID: 0xC01A
    USBD_VID: 0xC0CA
    USBD_BTH: 1
    # Optional name of
    USBD_PRODUCT_STRING: '"NimBLE"'
    USBD_BTH_DESCRIPTOR_STRING: '"NimBLE"'

    # Set to 1 if testing on Windows
    BLE_LL_HBD_FAKE_DUAL_MODE: 0

syscfg.vals.BLE_LL_HBD_FAKE_DUAL_MODE:
    USBD_PRODUCT_STRING: '"NimBLE (For Windows)"'
    USBD_BTH_DESCRIPTOR_STRING: '"NimBLE (For Windows)"'
    # Public address is required, it should be provisioned not set this way 
    BLE_LL_PUBLIC_DEV_ADDR: 0x010101010102
    BLE_TRANSPORT_EVT_SIZE: 257
```
Example target package file *targets/nordic_pca10059-blehci_nrf52dfu/pkg.yml*:
```yml
pkg.name: "targets/nordic_pca10059-blehci_nrf52dfu"
pkg.type: target
```

**FLASH_AREA_MBR** and **FLASH_AREA_NRF52_BOOTLOADER** are here to denote flash area not available to mynewt.

**FLASH_AREA_IMAGE_0** must start from **<span style="color:blue">0x1000</span>**

2. Build application with newt tool:
```shell
    newt build nordic_pca10059-blehci_nrf52dfu
```
3. Create hex file needed by nrfutil:
```shell
    cd bin/targets/nordic_pca10059-blehci_nrf52dfu/app/@apache-mynewt-nimble/apps/blehci/
    arm-none-eabi-objcopy -O ihex blehci.elf blehci.hex
```
4. Create package zip file for nrfutil:
```shell
    nrfutil pkg generate --hw-version 52 --sd-req 0 --application blehci.hex --application-version 1 blehci_nrf52dfu_package.zip
```
5. Enter bootloader mode by clicking reset button while user button is held. When Nordic Open DFU Bootloader is activated board red LED starts to blink and serial port is visible in the host system (**COM***x* for Windows or **/dev/ttyACM***x* for Linux).
6. Flash application package with nrfutil:
```shell
    nrfutil dfu usb-serial -p /dev/ttyACM0 --package blehci_nrf52dfu_package.zip
```
