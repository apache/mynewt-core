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
# Called $0 <binary> [identities ...]
#  - binary is the path to prefix to target binary, .elf appended to name is
#    the ELF file
#  - identities is the project identities string.
#
#
if [ $# -lt 1 ]; then
    echo "Need binary to download"
    exit 1
fi

BASENAME=$1
IS_BOOTLOADER=0
BIN2IMG=project/bin2img/bin/bin2img/bin2img.elf
VER=11.22.3333.0
VER_FILE=version.txt # or somewhere else

# Look for 'bootloader' from 2nd arg onwards
shift
while [ $# -gt 0 ]; do
    if [ $1 == "bootloader" ]; then
	IS_BOOTLOADER=1
    fi
    shift
done

if [ $IS_BOOTLOADER -eq 1 ]; then
    FLASH_OFFSET=0x00000000
    FILE_NAME=$BASENAME.elf.bin
else
    FLASH_OFFSET=0x00008000
    FILE_NAME=$BASENAME.elf.img
    if [ -f $VER_FILE ]; then
	VER=`echo $VER_FILE`
    fi
    echo "Version is >" $VER "<"
    $BIN2IMG $BASENAME.elf.bin $FILE_NAME $VER
    if [ "$?" -ne 0 ]; then
	exit 1
    fi
fi
echo "Downloading" $FILE_NAME "to" $FLASH_OFFSET

openocd -f hw/bsp/arduino_zero/arduino_zero.cfg -c init -c "reset halt" -c "flash write_image erase $FILE_NAME $FLASH_OFFSET" -c "reset run" -c shutdown

