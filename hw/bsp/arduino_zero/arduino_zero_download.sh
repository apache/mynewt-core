#!/bin/sh
#
# Called: $0 <binary> [identities...]
#  - binary is the path to prefix to target binary, .elf.bin appended to this
#    name is the raw binary format of the binary.
#  - identities is the project identities string. So you can have e.g. different
#    flash offset for bootloader identity
# 
#
if [ $# -lt 1 ]; then
    echo "Need binary to download"
    exit 1
fi

BASENAME=$1
IS_BOOTLOADER=1
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
    FLASH_OFFSET=0x00002000
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

openocd -f arduino_zero.cfg -c init -c "reset halt" -c "flash write_image erase $FILE_NAME $FLASH_OFFSET" -c "reset run" -c shutdown

