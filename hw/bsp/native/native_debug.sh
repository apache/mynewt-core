#!/bin/sh
#
# Called $0 <binary> [identities ...]
#  - binary is the path to prefix to target binary, .elf appended to name is
#    the ELF file
#  - identities is the project identities string.
#
#
if [ $# -lt 1 ]; then
    echo "Need binary to debug"
    exit 1
fi

FILE_NAME=$1.elf

echo "Debugging" $FILE_NAME

gdb $FILE_NAME
