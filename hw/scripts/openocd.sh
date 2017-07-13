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

CFG_RESET="reset halt"
GDB=arm-none-eabi-gdb

#
# FILE_NAME must contain the name of the file to load
# FLASH_OFFSET must contain the offset in flash where to place it
#
openocd_load () {
    OCD_CMD_FILE=.openocd_cmds

    echo "$EXTRA_JTAG_CMD" > $OCD_CMD_FILE
    echo "init" >> $OCD_CMD_FILE
    echo "$CFG_RESET" >> $OCD_CMD_FILE
    echo "$CFG_POST_INIT" >> $OCD_CMD_FILE
    echo "flash write_image erase $FILE_NAME $FLASH_OFFSET" >> $OCD_CMD_FILE

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

    openocd $CFG -f $OCD_CMD_FILE -c shutdown
    if [ $? -ne 0 ]; then
        exit 1
    fi
    rm $OCD_CMD_FILE
    return 0
}

#
# NO_GDB should be set if gdb should not be started
# FILE_NAME should point to elf-file being debugged
#
openocd_debug () {
    OCD_CMD_FILE=.openocd_cmds

    windows_detect

    echo "gdb_port 3333" > $OCD_CMD_FILE
    echo "telnet_port 4444" >> $OCD_CMD_FILE
    echo "$EXTRA_JTAG_CMD" >> $OCD_CMD_FILE

    if [ -z "$NO_GDB" ]; then
        if [ -z $FILE_NAME ]; then
            echo "Missing filename"
            exit 1
        fi
        if [ ! -f "$FILE_NAME" ]; then
            echo "Cannot find file" $FILE_NAME
            exit 1
        fi

        if [ $WINDOWS -eq 1 ]; then
            #
            # Launch openocd in a separate command interpreter, to make sure
            # it doesn't get killed by Ctrl-C signal from bash.
            #

            CFG=`echo $CFG | sed 's/\//\\\\/g'`
            $COMSPEC /C "start $COMSPEC /C openocd -l openocd.log $CFG -f $OCD_CMD_FILE -c init -c halt"
        else
            #
            # Block Ctrl-C from getting passed to openocd.
            #
            set -m
            openocd $CFG -f $OCD_CMD_FILE -c init -c halt >openocd.log 2>&1 &
            openocdpid=$!
            set +m
        fi

        GDB_CMD_FILE=.gdb_cmds

        echo "target remote localhost:3333" > $GDB_CMD_FILE
        if [ ! -z "$RESET" ]; then
            echo "mon reset halt" >> $GDB_CMD_FILE
        fi

        echo "$EXTRA_GDB_CMDS" >> $GDB_CMD_FILE

        if [ $WINDOWS -eq 1 ]; then
            FILE_NAME=`echo $FILE_NAME | sed 's/\//\\\\/g'`
            $COMSPEC /C "start $COMSPEC /C $GDB -x $GDB_CMD_FILE $FILE_NAME"
        else
            set -m
            $GDB -x $GDB_CMD_FILE $FILE_NAME
            set +m
            rm $GDB_CMD_FILE
            sleep 1
            if [ -d /proc/$openocdpid ] ; then
                kill -9 $openocdpid
            fi
        fi
    else
        # No GDB, wait for openocd to exit
        openocd $CFG -f $OCD_CMD_FILE -c init -c halt
        return $?
    fi

    if [ ! -x "$COMSPEC" ]; then
        rm $OCD_CMD_FILE
    fi
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
