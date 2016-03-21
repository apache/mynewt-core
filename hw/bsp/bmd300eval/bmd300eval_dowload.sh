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

FLASH_OFFSET=0x0
FILE_NAME=$2.elf.bin
JLINK_SCRIPT=.download.jlink

echo "Downloading" $FILE_NAME "to" $FLASH_OFFSET

cat > $JLINK_SCRIPT <<EOF
w 4001e504 1
loadbin $FILE_NAME $FLASH_OFFSET
q
EOF

msgs=`JLinkExe -device nRF52 -speed 4000 -if SWD $JLINK_SCRIPT 2>&1`

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

error=`echo $msgs | grep -i "not found"`
if [ -n "$error" ]; then
    exit 1
fi

exit 0
