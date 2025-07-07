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

. $CORE_PATH/hw/scripts/openocd.sh

CFG="-f $BSP_PATH/arduino_zero.cfg"

PROTECT_BOOT=0
if [ "$MFG_IMAGE" ]; then
    FLASH_OFFSET=0x00000000
    PROTECT_BOOT=1
elif [ "$BOOT_LOADER" ]; then
    PROTECT_BOOT=1
fi

if [ $PROTECT_BOOT -eq 1 ]; then
    # we will unprotect and reprotect our bootloader to ensure that its
    # safe. also Arduino Zero Pro boards looks like they come with the 
    # arduino bootloader protected via the NVM AUX
    CFG_POST_INIT="at91samd bootloader 0"

    # XXX reprotect todo
    #PROTECT_FLASH="at91samd bootloader 16384"
fi

common_file_to_load
openocd_load
openocd_reset_run
