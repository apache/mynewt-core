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

. $CORE_PATH/hw/scripts/common.sh

JLINK_GDB_SERVER=JLinkGDBServer

#
# FILE_NAME is the file to load
# FLASH_OFFSET is location in the flash
# JLINK_DEV is what we tell JLinkGDBServer this device to be
#
jlink_load () {
    GDB_CMD_FILE=.gdb_cmds
    GDB_OUT_FILE=.gdb_out

    windows_detect
    if [ $WINDOWS -eq 1 ]; then
	JLINK_GDB_SERVER=JLinkGDBServerCL
    fi
    if [ -z $FILE_NAME ]; then
        echo "Missing filename"
        exit 1
    fi
    if [ ! -f "$FILE_NAME" ]; then
	# tries stripping current path for readability
        FILE=${FILE_NAME##$(pwd)/}
	echo "Cannot find file" $FILE
	exit 1
    fi
    if [ -z $FLASH_OFFSET ]; then
        echo "Missing flash offset"
        exit 1
    fi

    echo "Downloading" $FILE_NAME "to" $FLASH_OFFSET

    # XXX for some reason JLinkExe overwrites flash at offset 0 when
    # downloading somewhere in the flash. So need to figure out how to tell it
    # not to do that, or report failure if gdb fails to write this file
    #
    echo "shell sh -c \"trap '' 2; $JLINK_GDB_SERVER $EXTRA_JTAG_CMD -device $JLINK_DEV -speed 4000 -if SWD -port 3333 -singlerun &\" " > $GDB_CMD_FILE
    echo "target remote localhost:3333" >> $GDB_CMD_FILE
    echo "mon reset" >> $GDB_CMD_FILE
    echo "restore $FILE_NAME binary $FLASH_OFFSET" >> $GDB_CMD_FILE

    # XXXX 'newt run' was not always updating the flash on nrf52dk. With
    # 'info reg' in place it seems to work every time. Not sure why.
    echo "info reg" >> $GDB_CMD_FILE
    echo "quit" >> $GDB_CMD_FILE

    msgs=`arm-none-eabi-gdb -x $GDB_CMD_FILE 2>&1`
    echo $msgs > $GDB_OUT_FILE

    rm $GDB_CMD_FILE

    # Echo output from script run, so newt can show it if things go wrong.
    # JLinkGDBServer always exits with non-zero error code, regardless of
    # whether there was an error during execution of it or not. So we cannot
    # use it.
    echo $msgs

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

    return 0
}

#
# FILE_NAME is the file to debug
# NO_GDB is set if we should not start gdb
# JLINK_DEV is what we tell JLinkGDBServer this device to be
# EXTRA_GDB_CMDS is for extra commands to pass to gdb
# RESET is set if we should reset the target at attach time
#
jlink_debug() {
    windows_detect
    if [ $WINDOWS -eq 1 ]; then
	JLINK_GDB_SERVER=JLinkGDBServerCL
    fi

    if [ -z "$NO_GDB" ]; then
        GDB_CMD_FILE=.gdb_cmds

        if [ -z $FILE_NAME ]; then
            echo "Missing filename"
            exit 1
        fi
        if [ ! -f "$FILE_NAME" ]; then
            echo "Cannot find file" $FILE_NAME
            exit 1
        fi

        echo "Debugging" $FILE_NAME

        if [ $WINDOWS -eq 1 ]; then
            #
            # Launch jlink server in a separate command interpreter, to make
            # sure it doesn't get killed by Ctrl-C signal from bash.
            #
            $COMSPEC /C "start $COMSPEC /C $JLINK_GDB_SERVER $EXTRA_JTAG_CMD -device $JLINK_DEV -speed 4000 -if SWD -port 3333 -singlerun"
        else
            #
            # Block Ctrl-C from getting passed to jlink server.
            #
            set -m
            $JLINK_GDB_SERVER $EXTRA_JTAG_CMD -device $JLINK_DEV -speed 4000 -if SWD -port 3333 -singlerun > /dev/null &
            set +m
        fi

        echo "target remote localhost:3333" > $GDB_CMD_FILE
        # Whether target should be reset or not
        if [ ! -z "$RESET" ]; then
            echo "mon reset" >> $GDB_CMD_FILE
            echo "si" >> $GDB_CMD_FILE
        fi
        echo "$EXTRA_GDB_CMDS" >> $GDB_CMD_FILE

	if [ $WINDOWS -eq 1 ]; then
	    FILE_NAME=`echo $FILE_NAME | sed 's/\//\\\\/g'`
	    $COMSPEC /C "start $COMSPEC /C arm-none-eabi-gdb -x $GDB_CMD_FILE $FILE_NAME"
	else
            arm-none-eabi-gdb -x $GDB_CMD_FILE $FILE_NAME
            rm $GDB_CMD_FILE
	fi
    else
        $JLINK_GDB_SERVER $EXTRA_JTAG_CMD -device $JLINK_DEV -speed 4000 -if SWD -port 3333 -singlerun
    fi
    return 0
}
