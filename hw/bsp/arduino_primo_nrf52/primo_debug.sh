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

if [ -z "$BIN_BASENAME" ]; then
    echo "Need binary to debug"
    exit 1
fi

USE_OPENOCD=0
FILE_NAME=$BIN_BASENAME.elf
GDB_CMD_FILE=.gdb_cmds

echo "Debugging" $FILE_NAME

# Look for 'openocd_debug' in FEATURES
for feature in $FEATURES; do
    if [ $feature = "openocd_debug" ]; then
        USE_OPENOCD=1
    fi
done

echo "target remote localhost:3333" > $GDB_CMD_FILE

if [ $USE_OPENOCD -eq 1 ]; then
    if [ -z "$BSP_PATH" ]; then
        echo "Need BSP path for openocd script location"
        exit 1
    fi

    #
    # Block Ctrl-C from getting passed to openocd.
    # Exit openocd when gdb detaches.
    #
    set -m
    openocd -s $BSP_PATH -f arduino_primo.cfg -c "$EXTRA_JTAG_CMD" -c "gdb_port 3333; telnet_port 4444; nrf52.cpu configure -event gdb-detach {resume;shutdown}" -c init -c halt &
    set +m
    # Whether target should be reset or not
    if [ ! -z "$RESET" ]; then
	echo "mon reset halt" >> $GDB_CMD_FILE
    fi
else
    #
    # Block Ctrl-C from getting passed to JLinkGDBServer
    set -m
    JLinkGDBServer -device nRF52 -speed 4000 -if SWD -port 3333 -singlerun > /dev/null &
    set +m
    # Whether target should be reset or not
    if [ ! -z "$RESET" ]; then
	echo "mon reset" >> $GDB_CMD_FILE
    fi
fi

arm-none-eabi-gdb -x $GDB_CMD_FILE $FILE_NAME
rm $GDB_CMD_FILE

