#!/bin/bash
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

#
# Download script for PIC32 BSPs (bsp.downloadscript).
#
# Called with following variables set:
#  - CORE_PATH is absolute path to @apache-mynewt-core
#  - BSP_PATH is absolute path to hw/bsp/bsp_name
#  - BIN_BASENAME is the path to prefix to target binary,
#    .elf appended to name is the ELF file
#  - FLASH_OFFSET, FLASH_AREA_SIZE flash area (application slot or
#    bootloader) to download to, from BSP flash map
#  - EXTRA_JTAG_CMD holds extra parameters to pass to programmer software
#  - MFG_IMAGE is "1" if this is a manufacturing image
#  - BOOT_LOADER is set if downloading a bootloader
#
# Syscfg values used:
#  - JLINK_TARGET device name (e.g. PIC32MZ2048EFM144), set by BSP
#  - MYNEWT_DOWNLOADER: jlink or mplab_ipe (PICkit, Snap, ICD)
#  - MCU_NO_BOOTLOADER_BUILD set when application is built to run without
#    bootloader
#

. $CORE_PATH/hw/scripts/jlink.sh

PIC32_DEVICE=${MYNEWT_VAL_JLINK_TARGET}
IMAGE_FLASH_ADDR=${FLASH_OFFSET}
IMAGE_FLASH_SIZE=${FLASH_AREA_SIZE}
PIC32_JLINK_SPEED=${PIC32_JLINK_SPEED:-12000}

# Runs JLinkExe with command file; fails if J-Link reported an error
pic32_jlink_run() {
    local out
    out=$(JLink${COMSPEC:+.}Exe -Device ${PIC32_DEVICE} -If ICSP $EXTRA_JTAG_CMD "$@" 2>&1)
    local rc=$?
    echo "$out"
    if [ $rc -ne 0 ] || echo "$out" | grep -q -E "Error occurred|Could not connect|Failed to"; then
        echo "J-Link operation failed"
        exit 1
    fi
}

pic32_jlink_load() {
    jlink_sn

    local script=${BIN_BASENAME}.jlink

    echo "r" > $script
    echo "h" >> $script
    if [ "$BOOT_LOADER" -o "$MYNEWT_VAL_MCU_NO_BOOTLOADER_BUILD" == "1" ]; then
        xc32-objcopy -O ihex ${BIN_BASENAME}.elf ${BIN_BASENAME}.hex || exit 1
        echo "loadfile ${BIN_BASENAME}.hex" >> $script
    else
        # J-Link erases only flash pages covered by the image, everything
        # else (bootloader, other slots, data areas) is left intact.
        cp ${BIN_BASENAME}.img ${BIN_BASENAME}.bin || exit 1
        echo "loadfile ${BIN_BASENAME}.bin ${IMAGE_FLASH_ADDR}" >> $script
    fi
    echo "q" >> $script

    pic32_jlink_run -AutoConnect 1 -speed ${PIC32_JLINK_SPEED} -CommandFile $script

    # Code started by debugger reset and go does not run reliably (first
    # context switch never happens). Pulse MCLR without connecting to the
    # target instead.
    echo "r0" > $script
    echo "sleep 200" >> $script
    echo "r1" >> $script
    echo "q" >> $script
    pic32_jlink_run -CommandFile $script
}

if [ -z "$PIC32_DEVICE" ]; then
    >&2 echo "Syscfg value JLINK_TARGET (PIC32 device name) not set"
    exit 1
fi
if [ "$MFG_IMAGE" ]; then
    >&2 echo "Manufacturing images are not supported for PIC32"
    exit 1
fi

case "${MYNEWT_VAL_MYNEWT_DOWNLOADER}" in
"mplab_ipe")
    . $CORE_PATH/hw/scripts/mplab_ipe.sh
    # ipecmd expects device name without PIC prefix
    MPLAB_DEVICE=${MYNEWT_VAL_MYNEWT_DOWNLOADER_MPLAB_DEVICE:-${PIC32_DEVICE#PIC}}
    ipe_load
    ;;
"" | "jlink")
    pic32_jlink_load
    ;;
*)
    >&2 echo "Downloader ${MYNEWT_VAL_MYNEWT_DOWNLOADER} not supported for PIC32"
    exit 1
    ;;
esac
