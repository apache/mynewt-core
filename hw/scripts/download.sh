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
if [ "$OS" = "Windows_NT" ]; then
    JLINKEXE=JLink.exe
else
    JLINKEXE=JLinkExe
fi

check_downloader() {
  if ! command -v $1 &> /dev/null; then
    >&2 echo "$1 not found."
    exit 1
  fi
}

if [ "$MFG_IMAGE" ]; then
  if [ -z "$MYNEWT_VAL_MYNEWT_DOWNLOADER_MFG_IMAGE_FLASH_OFFSET" ] ; then
    >&2 echo "Syscfg value MYNEWT_DOWNLOADER_MFG_IMAGE_FLASH_OFFSET not set"
    exit -1
  fi
  FLASH_OFFSET=${MYNEWT_VAL_MYNEWT_DOWNLOADER_MFG_IMAGE_FLASH_OFFSET}
fi

case "${MYNEWT_VAL_MYNEWT_DOWNLOADER}" in
  "ezflashcli")
    check_downloader ezFlashCLI
    . $CORE_PATH/hw/scripts/ezflashcli.sh
    common_file_to_load
    ezflashcli_load
    ;;
  "jlink")
    check_downloader $JLINKEXE
    . $CORE_PATH/hw/scripts/jlink.sh
    if [ -z "${MYNEWT_VAL_JLINK_TARGET}" ] ; then
      >&2 echo -e "\n\nSyscfg value MYNEWT_DOWNLOADER set to 'jlink' but JLINK_TARGET not set in syscfg."
      exit -1
    fi
    JLINK_DEV=${MYNEWT_VAL_JLINK_TARGET}
    common_file_to_load
    jlink_load
    ;;
  "nrfjprog")
    check_downloader nrfjprog
    . $CORE_PATH/hw/scripts/nrfjprog.sh
    common_file_to_load
    nrfjprog_load
    ;;
  "nrfutil")
    check_downloader nrfutil
    . $CORE_PATH/hw/scripts/nrfutil.sh
    common_file_to_load
    nrfutil_load
    ;;
  "openocd")
    check_downloader openocd
    . $CORE_PATH/hw/scripts/openocd.sh
    if [ -n "${MYNEWT_VAL_MYNEWT_DOWNLOADER_OPENOCD_INTERFACE}" ] ; then
      CFG="-f ${MYNEWT_VAL_MYNEWT_DOWNLOADER_OPENOCD_INTERFACE}"
    fi
    if [ -n "${MYNEWT_VAL_MYNEWT_DOWNLOADER_OPENOCD_BOARD}" ] ; then
      CFG="${CFG} -f ${MYNEWT_VAL_MYNEWT_DOWNLOADER_OPENOCD_BOARD}"
    fi
    if [ -n "${MYNEWT_VAL_MYNEWT_DOWNLOADER_OPENOCD_CFG}" ] ; then
      CFG="${CFG} -s $BSP_PATH -f ${MYNEWT_VAL_MYNEWT_DOWNLOADER_OPENOCD_CFG}"
    fi
    if [ -z "${CFG}" ] ; then
      >&2 echo -e "\n\nSyscfg value MYNEWT_DOWNLOADER set to 'openocd' but none of the following syscfg values is not set:"
      >&2 echo -e "\tMYNEWT_DOWNLOADER_OPENOCD_INTERFACE\n\tMYNEWT_DOWNLOADER_OPENOCD_BOARD\n\tMYNEWT_DOWNLOADER_OPENOCD_CFG"
      exit -1
    fi
    common_file_to_load
    openocd_load
    openocd_reset_run
    ;;
  "pyocd")
    check_downloader pyocd
    . $CORE_PATH/hw/scripts/pyocd.sh
    if [ -z "${MYNEWT_VAL_PYOCD_TARGET}" ] ; then
      >&2 echo -e "\n\nSyscfg value MYNEWT_DOWNLOADER set to 'pyocd' but PYOCD_TARGET not set in syscfg."
      >&2 echo -e "pyocd supported targets can be found with 'pyocd list --targets'\n"
      exit -1
    fi
    TARGET=${MYNEWT_VAL_PYOCD_TARGET}
    common_file_to_load
    pyocd_load
    ;;
  "stflash")
    check_downloader st-flash
    . $CORE_PATH/hw/scripts/stlink.sh
    common_file_to_load
    stlink_load
    ;;
  "stm32_programmer_cli")
    check_downloader STM32_Programmer_CLI
    . $CORE_PATH/hw/scripts/stm32_programmer.sh
    common_file_to_load
    stlink_load
    ;;
  *)
    >&2 echo "Unknown downloader $MYNEWT_VAL_MYNEWT_DOWNLOADER"
    exit 1
    ;;
esac
