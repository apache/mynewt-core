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

FLASH_OFFSET=0x0
FILE_NAME=$1.elf.bin
JLINK_SCRIPT=.download.jlink

echo "Downloading" $FILE_NAME "to" $FLASH_OFFSET

cat > $JLINK_SCRIPT <<EOF
w 4001e504 1
loadbin $FILE_NAME $FLASH_OFFSET
q
EOF

msgs=`JLinkExe -device nRF51422_xxAC -speed 4000 -if SWD $JLINK_SCRIPT`

# Echo output from script run, so newt can show it if things go wrong.
echo $msgs
rm $JLINK_SCRIPT

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

exit 0
