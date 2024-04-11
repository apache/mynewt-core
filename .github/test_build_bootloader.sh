#!/bin/bash -x

# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

EXIT_CODE=0

BSPS=$(ls repos/apache-mynewt-core/hw/bsp)
IGNORED_BSPS="ci40 dialog_cmac embarc_emsk hifive1 native-armv7 native-mips\
              olimex-pic32-emz64 olimex-pic32-hmz144 pic32mx470_6lp_clicker\
              pic32mz2048_wi-fire usbmkw41z native"

# native is supported only on Linux (mind the space)
if [ $RUNNER_OS != "Linux" ]; then
    IGNORED_BSPS+=" native"
fi

BSP_COUNT=0

for bsp in ${BSPS}; do
    # NOTE: do not remove the spaces around IGNORED_BSPS; it's required to
    #       match against the first and last entries
    if [[ " ${IGNORED_BSPS} " =~ [[:blank:]]${bsp}[[:blank:]] ]]; then
        echo "Skipping bsp=$bsp"
        continue
    fi

    # Limit build on non-Linux VMs to 10 BSPs
    if [ $RUNNER_OS != "Linux" ]; then
       ((BSP_COUNT++))

       if [ $BSP_COUNT -gt 10 ]; then
            echo "Skipping bsp=$bsp (limit reached)"
            continue
       fi
    fi


    echo "Testing bsp=$bsp"

    target="test-bootloader-$bsp"
    newt target delete -s -f $target &> /dev/null
    newt target create -s $target
    newt target set -s $target bsp="@apache-mynewt-core/hw/bsp/$bsp"
    newt target set -s $target app="@mcuboot/boot/mynewt"
    newt target set -s $target build_profile=optimized
    newt build -q $target

    rc=$?
    [[ $rc -ne 0 ]] && EXIT_CODE=$rc

    newt target delete -s -f $target
done

exit $EXIT_CODE
