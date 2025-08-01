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

jlink_sn () {
    if [ -n "$JLINK_SN" ]; then
        SRN_ARG="--snr $JLINK_SN"
    elif [ -n "$MYNEWT_VAL_JLINK_SN" ]; then
        SRN_ARG="--snr $MYNEWT_VAL_JLINK_SN"
    else
        SRN_ARG=""
    fi
}

#
# FILE_NAME must contain the name of the file to load
# FLASH_OFFSET must contain the offset in flash where to place it
#
nrfjprog_load () {
    if [ -z ${FILE_NAME} ]; then
        echo "Missing filename"
        exit 1
    fi
    if [ ! -f "${FILE_NAME}" ]; then
        # tries stripping current path for readability
        FILE=${FILE_NAME##$(pwd)/}
        echo "Cannot find file" $FILE
        exit 1
    fi
    if [ -z ${FLASH_OFFSET} ]; then
        echo "Missing flash offset"
        exit 1
    fi

    if [ -n "${MYNEWT_VAL_NRFJPROG_COPROCESSOR}" ] ; then
      NRFJPROG_ARG="${NRFJPROG_ARG} --coprocessor ${MYNEWT_VAL_NRFJPROG_COPROCESSOR}"
    fi
    arm-none-eabi-objcopy -O ihex -I binary --adjust-vma ${FLASH_OFFSET} ${FILE_NAME} ${BIN_BASENAME}.hex

    jlink_sn

    echo "Downloading" $FILE_NAME "to" $FLASH_OFFSET

    nrfjprog ${NRFJPROG_ARG} --program ${BIN_BASENAME}.hex --sectorerase --verify $SRN_ARG

    if [ $? -ne 0 ]; then
        exit 1
    fi
    nrfjprog --reset $SRN_ARG

    return 0
}
