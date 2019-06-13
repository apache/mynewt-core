#!/bin/bash
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

. $CORE_PATH/hw/scripts/jlink.sh

common_file_to_load

if [ "$MFG_IMAGE" ]; then
    echo "Manufacturing image not supported yet"
    exit 1
fi

parse_extra_jtag_cmd $EXTRA_JTAG_CMD

GDB_CMD_FILE=.gdb_load
JLINK_LOG_FILE=.jlink_log

# flash_loader build for this BSP
if [ -z $FLASH_LOADER ]; then
    FL_TGT=da1469x_flash_loader
    FLASH_LOADER=$BIN_ROOT/targets/$FL_TGT/app/apps/flash_loader/flash_loader.elf
fi
if [ ! -f $FLASH_LOADER ]; then
    FILE=${FLASH_LOADER##$(pwd)/}
    echo "Can't find flash_loader : $FILE"
    if [ ! -z $FL_TGT ]; then
	echo "Build $FL_TGT first"
    fi
    exit 1
fi

if [ ! -f $FILE_NAME ]; then
    FILE=${FILE_NAME##$(pwd)/}
    echo "Cannot find file" $FILE
    exit 1
fi

hex2le() {
    echo ${1:6:2}${1:4:2}${1:2:2}${1:0:2}
}

# XXX Write windows version also
file_size() {
    echo `wc -c $FILE_NAME | awk '{print $1}'`
}

if [ "$BOOT_LOADER" ]; then
    IMAGE_FILE=$FILE_NAME.img

    # allocate 9K for header
    dd if=/dev/zero bs=9k count=1 | LANG=C LC_CTYPE=C tr "\000" "\377" > ${IMAGE_FILE}

    # update product header
    HEX="5070 00200000 00200000 eb00a5a8 66000000 aa11 0300 014007c84e"
    echo -n ${HEX} | xxd -r -p | dd bs=1 of=${IMAGE_FILE} conv=notrunc

    # update image header
    SIZE=$(hex2le $(printf "%08x" $(file_size ${FILE_NAME})))
    CRC32=$(hex2le $(crc32 ${FILE_NAME}))
    HEX="5171 ${SIZE} ${CRC32} 00000000000000000000000000000000 00000000 00040000 aa22 0000 aa44 0000"
    echo -n ${HEX} | xxd -r -p | dd bs=1 of=${IMAGE_FILE} seek=8k conv=notrunc

    # append original image to header
    cat $FILE_NAME >> ${IMAGE_FILE}

    # use new image for flashing
    FILE_NAME=${IMAGE_FILE}
fi

FILE_SIZE=$(file_size $FILE_NAME)

cat > $GDB_CMD_FILE <<EOF
set pagination off
shell sh -c "trap '' 2; $JLINK_GDB_SERVER -device cortex-m33 -speed 4000 -if SWD -port $PORT -singlerun $EXTRA_JTAG_CMD > $JLINK_LOG_FILE 2>&1 &"
target remote localhost:$PORT
mon reset
mon halt
restore $FLASH_LOADER.bin binary 0x20000000
symbol-file $FLASH_LOADER
set *(int *)0x500000BC = 4
set \$sp=*(int *)0x20000000
set \$pc=*(int *)0x20000004
b main
c
d 1

source $CORE_PATH/compiler/gdbmacros/flash_loader.gdb
mon go

# Wait for flash_loader to get ready
printf "Waiting for flash loader to get ready...\n"
while fl_state != 1
end

fl_erase 0 $FLASH_OFFSET $FILE_SIZE
fl_program $FILE_NAME 0 $FLASH_OFFSET

# 'mon reset' does not appear to reset the board. Write to MCU register
# directly instead.
set *(int *)0x100C0050 = 1
quit

EOF

arm-none-eabi-gdb -x $GDB_CMD_FILE

# gdb appears to exit before jlinkgdbserver, so immediate
# execution of debug script (i.e. from 'newt run'), will fail due
# to 2 instances of jlinkgdbserver running. Slow exit from here for a bit.
#
sleep 1
