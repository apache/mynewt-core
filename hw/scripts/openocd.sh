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
. $CORE_PATH/hw/scripts/common.sh

#
# FILE_NAME must contain the name of the file to load
# FLASH_OFFSET must contain the offset in flash where to place it
#
openocd_load () {
    if [ -z $FILE_NAME ]; then
	echo "Missing filename"
	return 1
    fi
    if [ ! -f "$FILE_NAME" ]; then
	echo "Cannot find file" $FILE
	return 1
    fi
    if [ -z $FLASH_OFFSET ]; then
	echo "Missing flash offset"
	return 1
    fi
    if [ ! -z "$EXTRA_JTAG_CMD" ]; then
	CFG="$CFG -c $EXTRA_JTAG_CMD"
    fi

    echo "Downloading" $FILE_NAME "to" $FLASH_OFFSET

    openocd $CFG -c init -c "reset halt" -c "flash write_image erase $FILE_NAME $FLASH_OFFSET" -c shutdown
    return $?
}

#
# NO_GDB should be set if gdb should not be started
# FILE_NAME should point to elf-file being debugged
#
openocd_debug () {
    OCD_CMD_FILE=.openocd_cmds

    echo "gdb_port 3333" > $OCD_CMD_FILE
    echo "telnet_port 4444" >> $OCD_CMD_FILE
    echo "$EXTRA_JTAG_CMD" >> $OCD_CMD_FILE

    if [ -z "$NO_GDB" ]; then
	if [ -z $FILE_NAME ]; then
	    echo "Missing filename"
	    return 1
	fi
	if [ ! -f "$FILE_NAME" ]; then
	    echo "Cannot find file" $FILE_NAME
	    return 1
	fi

	#
	# Block Ctrl-C from getting passed to openocd.
	#
	set -m
	openocd $CFG -f $OCD_CMD_FILE -c init -c halt &
	set +m

    	GDB_CMD_FILE=.gdb_cmds

    	echo "target remote localhost:3333" > $GDB_CMD_FILE
	if [ ! -z "$RESET" ]; then
    	    echo "mon reset halt" >> $GDB_CMD_FILE
	fi
	arm-none-eabi-gdb -x $GDB_CMD_FILE $FILE_NAME
	rm $GDB_CMD_FILE
    else
	# No GDB, wait for openocd to exit
	openocd $CFG -f $OCD_CMD_FILE -c init -c halt
    fi


    rm $OCD_CMD_FILE
    return 0
}

openocd_halt () {
    openocd $CFG -c init -c "halt" -c shutdown
    return $?
}

openocd_reset_run () {
    openocd $CFG -c init -c "reset run" -c shutdown
    return $?
}
