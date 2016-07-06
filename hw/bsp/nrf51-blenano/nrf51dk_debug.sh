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
# Called: $0 <bsp_directory_path> <binary> [features...]
#  - bsp_directory_path is absolute path to hw/bsp/bsp_name
#  - binary is the path to prefix to target binary, .elf.bin appended to this
#    name is the raw binary format of the binary.
#  - features is the project identities string. So you can have e.g. different
#    flash offset for bootloader 'feature'
# 
#
if [ $# -lt 2 ]; then
    echo "Need binary to download"
    exit 1
fi

MYPATH=$1
FILE_NAME=$2.elf
GDB_CMD_FILE=.gdb_cmds

echo "Debugging" $FILE_NAME

set -m
openocd -s $MYPATH -f cmsis-dap.cfg -f nrf51.cfg -c "nrf51.cpu configure -event gdb-detach {shutdown}" -c init -c "reset halt" &
set +m

echo "target remote localhost:3333" > $GDB_CMD_FILE

arm-none-eabi-gdb -x $GDB_CMD_FILE $FILE_NAME

rm $GDB_CMD_FILE

