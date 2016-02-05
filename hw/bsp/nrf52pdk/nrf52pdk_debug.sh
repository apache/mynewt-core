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

FILE_NAME=$1.elf
GDB_CMD_FILE=.gdb_cmds

echo "Debugging" $FILE_NAME

# Monitor mode. Background process gets it's own process group.
set -m
JLinkGDBServer -device nRF52 -speed 4000 -if SWD -port 3333 -singlerun &
set +m

echo "target remote localhost:3333" > $GDB_CMD_FILE

arm-none-eabi-gdb -x $GDB_CMD_FILE $FILE_NAME

rm $GDB_CMD_FILE

