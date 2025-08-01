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
stlink_sn () {
    if [ -n "$STLINK_SN" ]; then
        if [ "$STLINK_USE_STM32_PROGRAMMER_CLI" == 1 ] ; then
            EXTRA_JTAG_CMD="sn=$STLINK_SN " $EXTRA_JTAG_CMD
        else
            EXTRA_JTAG_CMD="--serial $STLINK_SN " $EXTRA_JTAG_CMD
        fi
    elif [ -n "$MYNEWT_VAL_STLINK_SN" ]; then
        if [ "$STLINK_USE_STM32_PROGRAMMER_CLI" == 1 ] ; then
            EXTRA_JTAG_CMD="sn=$MYNEWT_VAL_STLINK_SN " $EXTRA_JTAG_CMD
        else
            EXTRA_JTAG_CMD="--serial $MYNEWT_VAL_STLINK_SN " $EXTRA_JTAG_CMD
        fi
    fi
}

stlink_load () {
    stlink_sn

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

    if [ "$STLINK_USE_STM32_PROGRAMMER_CLI" == 1 ] ; then
        STM32_Programmer_CLI -c port=SWD $EXTRA_JTAG_CMD -d $FILE_NAME $FLASH_OFFSET -rst
    else
        st-flash $EXTRA_JTAG_CMD --reset write $FILE_NAME $FLASH_OFFSET
    fi
    if [ $? -ne 0 ]; then
        exit 1
    fi
    return 0
}

#
# NO_GDB should be set if gdb should not be started
# FILE_NAME should point to elf-file being debugged
#
stlink_debug () {
    windows_detect
    gdb_detect

    PORT=3333

    parse_extra_jtag_cmd $EXTRA_JTAG_CMD

    if [ -z "$NO_GDB" ]; then
        if [ -z $FILE_NAME ]; then
            echo "Missing filename"
            exit 1
        fi
        if [ ! -f "$FILE_NAME" ]; then
            echo "Cannot find file" $FILE_NAME
            exit 1
        fi

        #
        # Block Ctrl-C from getting passed to st-util.
        #
        set -m
        st-util --no-reset -p $PORT >stlink.log 2>&1 &
        stpid=$!
        set +m

        GDB_CMD_FILE=.gdb_cmds

        echo "target remote localhost:$PORT" > $GDB_CMD_FILE
        if [ ! -z "$RESET" ]; then
            echo "mon reset halt" >> $GDB_CMD_FILE
        fi

        echo "$EXTRA_GDB_CMDS" >> $GDB_CMD_FILE

        set -m
        $GDB -x $GDB_CMD_FILE $FILE_NAME
        set +m
        rm $GDB_CMD_FILE
        sleep 1
        if [ -d /proc/$stpid ] ; then
            kill -9 $stpid
        fi
    else
        # No GDB, wait for st-util to exit
        st-util --no-reset -p $PORT
        return $?
    fi

    return 0
}
