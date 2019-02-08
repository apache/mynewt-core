#!/bin/sh
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

# Called with following variables set:
#  - CORE_PATH is absolute path to @apache-mynewt-core
#  - BSP_PATH is absolute path to hw/bsp/bsp_name
#  - BIN_BASENAME is the path to prefix to target binary,
#    .elf appended to name is the ELF file
#  - IMAGE_SLOT is the image slot to download to (for non-mfg-image, non-boot)
#  - FEATURES holds the target features string
#  - EXTRA_JTAG_CMD holds extra parameters to pass to jtag software
#  - MFG_IMAGE is "1" if this is a manufacturing image
#  - FLASH_OFFSET contains the flash offset to download to
#  - BOOT_LOADER is set if downloading a bootloader

if [ -z "$CLI_PROGRAMMER" ]; then
    CLI_PROGRAMMER=cli_programmer
fi

if [ -z "$MKIMAGE" ]; then
    MKIMAGE=mkimage
fi

. ${CORE_PATH}/hw/scripts/common.sh
common_file_to_load

JLinkGDBServer -device cortex-m33 -if swd -speed 4000 -if swd -port 3333 -singlerun &

if [ "$BOOT_LOADER" ]; then
    VER_FILE=`mktemp`
    IMG_FILE=`mktemp`

    trap "rm ${VER_FILE} ${IMG_FILE}" EXIT

    DATE=$(date -r ${FILE_NAME} +"%Y-%m-%d %H:%M")
    echo "#define BLACKORCA_SW_VERSION \"v_1.0.0.0\"" >> ${VER_FILE}
    echo "#define BLACKORCA_SW_VERSION_DATE \"${DATE}\"" >> ${VER_FILE}
    ${MKIMAGE} d2522 ${FILE_NAME} ${VER_FILE} ${IMG_FILE}

    ${CLI_PROGRAMMER} -p 3333 gdbserver write_qspi 0x2000 ${IMG_FILE}
else
    ${CLI_PROGRAMMER} -p 3333 gdbserver write_qspi ${FLASH_OFFSET} ${FILE_NAME}
fi
