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

CONNECTION_MODE_DEFAULT="halt"

#
# FILE_NAME must contain the name of the file to load
# FLASH_OFFSET must contain the offset in flash where to place it
# TARGET must be correectly set by the BSP; check `pyocd list --targets`
# CONNECTION_MODE can be one of "attach", "halt" (default), "pre-reset", "under-reset"
#
pyocd_sn() {
    if [ -n "$JLINK_SN" ]; then
        UID_ARG="-u $JLINK_SN"
    elif [ -n "$STLINK_SN" ]; then
        UID_ARG="-u $STLINK_SN"
    elif [ -n "$CMSIS_DAP_SN" ]; then
        UID_ARG="-u $CMSIS_DAP_SN"
    elif [ -n "$MYNEWT_VAL_JLINK_SN" ]; then
        UID_ARG="-u $MYNEWT_VAL_JLINK_SN"
    elif [ -n "$MYNEWT_VAL_STLINK_SN" ]; then
        UID_ARG="-u $MYNEWT_VAL_STLINK_SN"
    elif [ -n "$MYNEWT_VAL_CMSIS_DAP_SN" ]; then
        UID_ARG="-u $MYNEWT_VAL_CMSIS_DAP_SN"
    else
        UID_ARG=""
    fi
}

pyocd_load () {
    if [ -z $TARGET ]; then
        echo "Missing TARGET (get a list with 'pyocd list --targets')"
        exit 1
    fi

    if [ -z $CONNECTION_MODE ]; then
        CONNECTION_MODE=$CONNECTION_MODE_DEFAULT
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

    pyocd_sn

    echo "Downloading" $FILE_NAME "to" $FLASH_OFFSET

    pyocd flash -e sector -M $CONNECTION_MODE -t $TARGET $FILE_NAME@$FLASH_OFFSET --format bin $UID_ARG
    if [ $? -ne 0 ]; then
        exit 1
    fi
    return 0
}

#
# NO_GDB should be set if gdb should not be started
# FILE_NAME should point to elf-file being debugged
# TARGET must be correectly set by the BSP; check `pyocd list --targets`
# CONNECTION_MODE can be one of "attach", "halt" (default), "pre-reset", "under-reset"
#
pyocd_debug () {
    windows_detect
    gdb_detect
    PORT=3333

    parse_extra_jtag_cmd $EXTRA_JTAG_CMD

    if [ -z $TARGET ]; then
        echo "Missing TARGET (get a list with 'pyocd list --targets')"
        exit 1
    fi

    if [ -z $CONNECTION_MODE ]; then
        CONNECTION_MODE=$CONNECTION_MODE_DEFAULT
    fi

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
        # Block Ctrl-C from getting passed to pyocd.
        #
        set -m
        pyocd gdbserver -M $CONNECTION_MODE -t $TARGET -p $PORT >pyocd.log 2>&1 &
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
        # No GDB, wait for pyocd to exit
        pyocd gdbserver -M $CONNECTION_MODE -t $TARGET -p $PORT
        return $?
    fi

    return 0
}
