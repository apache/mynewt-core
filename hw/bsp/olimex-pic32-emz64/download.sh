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

if [ "${HWTOOL}" = "pickit3" ]; then
  . $CORE_PATH/hw/scripts/mplab_mdb.sh

  export DEVICE=PIC32MZ2048EFH064

  mdb_load

else

  echo "r" > script
  echo "h" >> script
  if [ "$BOOT_LOADER" -o "$MYNEWT_VAL_MCU_NO_BOOTLOADER_BUILD" == "1" ]; then
    xc32-objcopy -O ihex ${BIN_BASENAME}.elf ${BIN_BASENAME}.hex
    echo "loadfile ${BIN_BASENAME}.hex" >> script
  else
    cp ${BIN_BASENAME}.img ${BIN_BASENAME}.bin
    echo "loadfile ${BIN_BASENAME}.bin 0x1d000000" >> script
  fi
  echo "r" >> script
  echo "g" >> script
  echo "q" >> script

  JLink${COMSPEC:+.}Exe  -AutoConnect 1 -Device PIC32MZ2048EFH064 -If ICSP -speed 12000 -CommandFile script
fi
