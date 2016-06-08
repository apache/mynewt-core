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

# Called: $0 <bsp_directory_path> <binary> [features...]
#  - bsp_directory_path is absolute path to hw/bsp/bsp_name
#  - binary is the path to prefix to target binary, .elf.bin appended to this
#    name is the raw binary format of the binary.
#  - features are the target features. So you can have e.g. different
#    flash offset for bootloader 'feature'
#
#

if [ $# -lt 2 ]; then
    echo "Need binary to download"
    exit 1
fi

IS_BOOTLOADER=0
USE_OPENOCD=0
MYPATH=$1
BASENAME=$2
GDB_CMD_FILE=.gdb_cmds

# Look for 'bootloader' from 3rd arg onwards
shift
shift
while [ $# -gt 0 ]; do
    if [ $1 = "bootloader" ]; then
        IS_BOOTLOADER=1
    fi
    if [ $1 = "openocd_debug" ]; then
	USE_OPENOCD=1
    fi
    shift
done

if [ $IS_BOOTLOADER -eq 1 ]; then
    FLASH_OFFSET=0x0
    FILE_NAME=$BASENAME.elf.bin
else
    FLASH_OFFSET=0x8000
    FILE_NAME=$BASENAME.img
fi

echo "Downloading" $FILE_NAME "to" $FLASH_OFFSET

if [ $USE_OPENOCD -eq 1 ]; then
    #
    # XXXX note that this is using openocd through STM32, with openocd
    # which has been patched to support nrf52 flash.
    #
    openocd -s $MYPATH -f arduino_primo.cfg -c init -c "reset halt" -c "flash write_image erase $FILE_NAME $FLASH_OFFSET" -c "reset run" -c shutdown
else
    echo "shell /bin/sh -c 'trap \"\" 2;JLinkGDBServer -device nRF52 -speed 4000 -if SWD -port 3333 -singlerun' & " > $GDB_CMD_FILE
    echo "target remote localhost:3333" >> $GDB_CMD_FILE
    echo "restore $FILE_NAME binary $FLASH_OFFSET" >> $GDB_CMD_FILE
    echo "quit" >> $GDB_CMD_FILE

    msgs=`arm-none-eabi-gdb -x $GDB_CMD_FILE 2>&1`
    echo $msgs > .gdb_out

    rm $GDB_CMD_FILE

    # Echo output from script run, so newt can show it if things go wrong.
    echo $msgs

    error=`echo $msgs | grep error`
    if [ -n "$error" ]; then
	exit 1
    fi

    error=`echo $msgs | grep -i failed`
    if [ -n "$error" ]; then
	exit 1
    fi

    error=`echo $msgs | grep -i "unknown / supported"`
    if [ -n "$error" ]; then
	exit 1
    fi

    error=`echo $msgs | grep -i "not found"`
    if [ -n "$error" ]; then
	exit 1
    fi
fi
