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

# Check if Ctrl-AP APPROTECTSTATUS is enabled

trap cleanup 0 1 2 3 4 15 INT
cleanup()
{
	# exit : to ensure the script
	# doesn't contiue to run if terminated
	# during sleep
	exit 0
}

approtect_script=$MYNEWT_PROJECT_ROOT/hw/bsp/nordic_pca10090/approtect_detect
targetpoweron_script=$MYNEWT_PROJECT_ROOT/hw/bsp/nordic_pca10090/targetpoweron

if [ ! -f  $approtect_script ]; then
	echo "$approtect_script not found"
	exit 0
fi

check_approtect_status() {
	echo "check_approtect_status..."
	status=$(JLinkExe -device "CORTEX-M33" -speed 4000 -if SWD \
		-CommandFile $approtect_script | grep 'register 3' \
		| awk -F " " 'END{print $NF}')

	if [ -z $status ] ; then
		echo "Can't read APPROTECTSTATUS"
	elif [ $status = "0x00000000" ] ; then
		echo "JTAG_LOCKOUT is enabled, continuing will erase flash, press Ctrl+C to abort"
		echo "waiting 5s..."
		sleep 5
	fi
	return 0
}

# Turn the power on via the CommandFile and get  target voltage
get_target_voltage() {
	voltage=$(JLinkExe  -device "CORTEX-M33" -speed 1000 -if SWD \
		-CommandFile $targetpoweron_script | \
		sed -n 's/VTref\s*=\s*\(.*\)\.\(.*\)V/\1\2/p')
	echo $voltage
}

check_approtect_status
