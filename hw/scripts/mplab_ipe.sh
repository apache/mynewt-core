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

. "$CORE_PATH"/hw/scripts/common.sh

#
# Programming with Microchip MPLAB IPE command line tool (ipecmd) using
# PICkit 3/4/5, Snap or ICD programmers.
#
# Variables used:
#  - MPLAB_DEVICE device name as accepted by ipecmd -P option
#    (e.g. 32MZ2048EFM144), must be set by caller
#  - BIN_BASENAME, BOOT_LOADER, MFG_IMAGE as set by newt
#  - MYNEWT_VAL_MCU_NO_BOOTLOADER_BUILD set when application is built
#    to run without bootloader
#  - MYNEWT_VAL_MYNEWT_DOWNLOADER_MPLAB_TOOL programmer (PK3, PK4, SNAP, ...)
#    can be overridden with MPLAB_TOOL environment variable
#  - MYNEWT_VAL_MYNEWT_DEBUGGER_SN programmer serial number, needed when
#    more than one programmer is connected; can be overridden with
#    MPLAB_TOOL_SN environment variable
#  - IMAGE_FLASH_ADDR physical flash address of application image
#    (required for images with header)
#  - IPECMD path to ipecmd, if not set it is searched in PATH and in
#    default MPLAB X installation directories
#  - EXTRA_JTAG_CMD additional ipecmd options (e.g. -W to power target
#    from programmer)
#

ipe_find_ipecmd() {
    if [ -n "$IPECMD" ]; then
        return
    fi

    IPECMD=$(command -v ipecmd.exe || command -v ipecmd.sh || command -v ipecmd)
    if [ -n "$IPECMD" ]; then
        return
    fi

    # Newest MPLAB X version from default installation places
    for d in "${PROGRAMFILES:+$(cygpath -u "$PROGRAMFILES" 2>/dev/null)/Microchip/MPLABX}" \
             "/c/Program Files/Microchip/MPLABX" "/opt/microchip/mplabx" \
             "/Applications/microchip/mplabx"; do
        for f in $(ls -d "$d"/v*/mplab_platform/mplab_ipe/ipecmd.{exe,sh} 2>/dev/null | sort -V -r); do
            IPECMD=$f
            return
        done
    done

    echo "ipecmd not found, set IPECMD or add MPLAB IPE directory to PATH"
    exit 1
}

#
# Converts image to hex file with physical addresses as expected by ipecmd.
# Sets HEX_FILE, IPE_MEMORY (memory regions to program) and IPE_ERASE.
#
# Note: when programming, ipecmd (at least with PICkit 3) either erases
# whole device first or (with -OH) requires programmed memory to be blank,
# there is no erase of selected region only.
# Full images (bootloader, manufacturing image or application built with
# MCU_NO_BOOTLOADER_BUILD) are programmed after full device erase.
# Application image is programmed without erase so bootloader is not
# lost, this works only when application area is blank (right after
# bootloader was programmed) or already holds the same image.
#
ipe_prepare_hex() {
    if [ "$BOOT_LOADER" -o "$MFG_IMAGE" -o "$MYNEWT_VAL_MCU_NO_BOOTLOADER_BUILD" == "1" ]; then
        # ELF contains boot flash and configuration words, xc32-bin2hex
        # converts virtual addresses to physical ones.
        xc32-bin2hex "${BIN_BASENAME}".elf || exit 1
        HEX_FILE=${BIN_BASENAME}.hex
        IPE_MEMORY=
        IPE_ERASE=
    else
        if [ -z "$IMAGE_FLASH_ADDR" ]; then
            echo "IMAGE_FLASH_ADDR not set"
            exit 1
        fi
        HEX_FILE=${BIN_BASENAME}.img.hex
        xc32-objcopy -I binary -O ihex --change-addresses ${IMAGE_FLASH_ADDR} \
            "${BIN_BASENAME}".img "${HEX_FILE}" || exit 1
        # Program memory only, without erase, leave boot flash (bootloader)
        # intact. Range limits programming and verification to application
        # area, other flash areas may hold data.
        IPE_MEMORY=P
        if [ -n "$IMAGE_FLASH_SIZE" ]; then
            IPE_MEMORY=$(printf "P0x%X-0x%X" $((IMAGE_FLASH_ADDR)) \
                         $((IMAGE_FLASH_ADDR + IMAGE_FLASH_SIZE - 1)))
        fi
        IPE_ERASE=-OH
    fi
}

#
# Runs ipecmd from BUILD_DIR (ipecmd writes log files to current directory).
# Output is stored in IPE_OUT. Returns 0 when operation succeeded.
#
ipe_run() {
    local args="-P${MPLAB_DEVICE} -TP${TOOL} $*"
    if [ -n "$TOOL_SN" ]; then
        args="$args -TS${TOOL_SN}"
    fi
    IPE_OUT=$(cd "${BUILD_DIR}" && "$IPECMD" $args $EXTRA_JTAG_CMD 2>&1)
    local rc=$?
    echo "$IPE_OUT"
    # ipecmd does not always report failures with exit code
    [ $rc -eq 0 ] && echo "$IPE_OUT" | grep -q -i -E "(Program|Operation) Succeeded"
}

# Native path for ipecmd (MSYS does not convert path glued to -F)
ipe_native_path() {
    if command -v cygpath > /dev/null; then
        cygpath -m "$1"
    else
        echo "$1"
    fi
}

#
# Replaces area <start> <size> (hex) in device hex file <dev_hex> with
# content of <app_hex> and writes result to <out_hex>. Data records inside
# the area are dropped (area is left erased except for application data).
# Records produced by ipecmd read are 16 bytes and aligned so filtering
# by record start address is exact.
#
ipe_merge_hex() {
    awk -v start="$3" -v size="$4" -v app="$2" '
    function hex2dec(h,   i, n, c) {
        n = 0
        h = toupper(h)
        sub(/^0X/, "", h)
        for (i = 1; i <= length(h); i++) {
            c = index("0123456789ABCDEF", substr(h, i, 1)) - 1
            n = n * 16 + c
        }
        return n
    }
    BEGIN { s = hex2dec(start); e = s + hex2dec(size); base = 0 }
    {
        sub(/\r$/, "")
        type = substr($0, 8, 2)
        if (type == "04") {
            base = hex2dec(substr($0, 10, 4)) * 65536
        } else if (type == "00") {
            addr = base + hex2dec(substr($0, 4, 4))
            if (addr >= s && addr < e) {
                next
            }
        } else if (type == "01") {
            next
        }
        print
    }
    END {
        while ((getline line < app) > 0) {
            sub(/\r$/, "", line)
            print line
        }
    }' "$1" > "$5"
}

ipe_load() {
    if [ -z "$MPLAB_DEVICE" ]; then
        echo "MPLAB_DEVICE not set"
        exit 1
    fi

    ipe_find_ipecmd
    ipe_prepare_hex

    TOOL=${MPLAB_TOOL:-${MYNEWT_VAL_MYNEWT_DOWNLOADER_MPLAB_TOOL:-PK4}}
    TOOL_SN=${MPLAB_TOOL_SN:-$MYNEWT_VAL_MYNEWT_DEBUGGER_SN}

    BUILD_DIR=$(cd "$(dirname "${HEX_FILE}")" && pwd)
    HEX_PATH=$(ipe_native_path "${BUILD_DIR}/$(basename "${HEX_FILE}")")

    echo "Downloading ${HEX_FILE} with ${TOOL}"
    # -OL: release device from reset after programming
    if ipe_run -F"${HEX_PATH}" -M${IPE_MEMORY} ${IPE_ERASE} -OL; then
        return
    fi

    # PICkit 3 refuses non-blank memory, Snap/PICkit 4 program it and fail
    # verification (flash bits can only be cleared without erase).
    # ipecmd can't erase only part of flash, so whole device is read,
    # application area is replaced and everything is programmed back after
    # full erase. Bootloader, configuration words and other flash areas
    # (file systems, logs) are restored from what was read.
    if [ -z "$IPE_ERASE" ] || ! echo "$IPE_OUT" | grep -q -i -E "memory is not blank|expected 0x"; then
        echo "Programming failed"
        exit 1
    fi
    if [ -z "$IMAGE_FLASH_SIZE" -o "$MPLAB_NO_READ_BACK" == "1" ]; then
        echo "Application area is not blank, ipecmd can't program it without"
        echo "erasing whole device. Load bootloader first, use manufacturing"
        echo "image or build with MCU_NO_BOOTLOADER_BUILD."
        echo "Programming failed"
        exit 1
    fi

    echo "Application area is not blank, reading device to preserve other areas"
    DEV_HEX=device_read.hex
    rm -f "${BUILD_DIR}/${DEV_HEX}"
    # ipecmd accepts only file name relative to current directory for -GF
    if ! ipe_run -GF${DEV_HEX} || [ ! -s "${BUILD_DIR}/${DEV_HEX}" ]; then
        echo "Device read failed, nothing was erased"
        exit 1
    fi

    MERGED_HEX=${BUILD_DIR}/$(basename "${BIN_BASENAME}").merged.hex
    ipe_merge_hex "${BUILD_DIR}/${DEV_HEX}" "${BUILD_DIR}/$(basename "${HEX_FILE}")" \
        ${IMAGE_FLASH_ADDR} ${IMAGE_FLASH_SIZE} "${MERGED_HEX}" || exit 1

    echo "Programming application merged with device content"
    if ! ipe_run -F$(ipe_native_path "${MERGED_HEX}") -M -OL; then
        echo "Programming failed, device content was saved in ${BUILD_DIR}/${DEV_HEX}"
        exit 1
    fi
}
