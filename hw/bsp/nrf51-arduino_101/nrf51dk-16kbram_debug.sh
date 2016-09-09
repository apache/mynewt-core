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
#  - BSP_PATH is absolute path to hw/bsp/bsp_name
#  - BIN_BASENAME is the path to prefix to target binary,
#    .elf appended to name is the ELF file
#  - FEATURES holds the target features string
#  - EXTRA_JTAG_CMD holds extra parameters to pass to jtag software
#  - RESET set if target should be reset when attaching
#
if [ -z "$BIN_BASENAME" ]; then
    echo "Need binary to debug"
    exit 1
fi

FILE_NAME=$BIN_BASENAME.elf
GDB_CMD_FILE=.gdb_cmds

echo "Debugging" $FILE_NAME

set -m
JLinkGDBServer -device nRF51422_xxAC -speed 4000 -if SWD -port 3333 -singlerun > /dev/null &
set +m

echo "target remote localhost:3333" > $GDB_CMD_FILE

# Whether target should be reset or not
if [ ! -z "$RESET" ]; then
    echo "mon reset" >> $GDB_CMD_FILE
fi

arm-none-eabi-gdb --tui -x $GDB_CMD_FILE $FILE_NAME

rm $GDB_CMD_FILE
