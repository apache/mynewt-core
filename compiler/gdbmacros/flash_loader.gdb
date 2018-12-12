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

#
# Helpers to do things via flash_loader app.
#

#
# Check that the flash_loader is running
#
define fl_ping
	if fl_state != 1 || fl_cmd != 0
		printf "flash_loader not ready\n"
	else
		# Clear old RC
		set fl_cmd_rc = 0
		# ping command
		set fl_cmd = 1
		while fl_cmd_rc == 0

		end
		if fl_cmd_rc == 1
			printf "Ping ok!\n"
		else
			printf "Ping error: %d\n", fl_cmd_rc
		end
	end
end

document fl_ping
usage: fl_ping
Exchange a message with flash_loader program, verifying that it is ready
for commands
end

#
# Erase area
#
define fl_erase
	fl_ping
	if fl_cmd_rc == 1
		# Clear old RC
		set fl_cmd_rc = 0
		# Set parameters; flash dev, flash address and number of bytes
		set fl_cmd_flash_id = $arg0
		set fl_cmd_flash_addr = $arg1
		set fl_cmd_amount = $arg2

		# erase command
		set fl_cmd = 3
		while fl_cmd_rc == 0

		end
		if fl_cmd_rc == 1
			printf "Erase ok!\n"
		else
			printf "Erase error: %d\n", fl_cmd_rc
		end
	end
end

document fl_erase
usage: fl_erase <id> <offset> <cnt>
Asks flash_loader to erase a region of flash.
  id     - flash identifier as specified for this BSP
  offset - offset to this flash
  cnt    - number of bytes to erase (aligns to sector boundary)
end

#
# Load a file to a specific flash location
#
define fl_file_sz
       shell echo -n set \$file_sz= > /tmp/foo.gdb
       shell wc -c $arg0 | awk '{print $1}' >> /tmp/foo.gdb
       source /tmp/foo.gdb
       shell rm /tmp/foo.gdb
end

define fl_program
	fl_ping
	if fl_cmd_rc == 1
		fl_file_sz $arg0

		set $buf_sz = fl_cmd_data_sz
		set $fl_off = $arg2
		set $off = 0

		set fl_cmd_flash_id = $arg1

		while fl_cmd_rc == 1 && $off < $file_sz
			if $off + $buf_sz > $file_sz
				set $buf_sz = $file_sz - $off
			end

			printf " 0x%x %d\n", $fl_off + $off, $buf_sz

			set fl_cmd_flash_addr = $fl_off + $off
			set fl_cmd_amount = $buf_sz

			# XXX note computation of restore command dst address
			set $taddr = fl_cmd_data - $off
			set $end = $off + $buf_sz
			restore $arg0 binary $taddr $off $end

			# load and verify command
			set fl_cmd = 5

			# wait for flash write to start
			while fl_cmd != 0

			end
			set $off = $end
		end
		while fl_state != 1

		end
		if fl_cmd_rc == 1
			printf "Done\n"
		else
			printf "Error during flash program %d\n", fl_cmd_rc
		end
	end
end

document fl_program
usage: fl_program <file> <id> <offset>
Asks flash_loader to load the given file to flash.
  file    - name of the file to load
  id     - flash identifier as specified for this BSP
  offset - offset to this flash
end

define fl_load
       fl_file_sz $arg0

       fl_erase $arg1 $arg2 $file_sz
       fl_program $arg0 $arg1 $arg2
end

document fl_load
usage: fl_load <file> <id> <offset>
Asks flash_loader to erase flash area, and then load the file there.
  file    - name of the file to load
  id     - flash identifier as specified for this BSP
  offset - offset to this flash
end

define fl_dump
	fl_ping
	if fl_cmd_rc == 1
		set $file_sz = $arg3
		set $buf_sz = fl_cmd_data_sz
		set $fl_off = $arg2
		set $off = 0

		set fl_cmd_flash_id = $arg1

		shell rm $arg0
		while fl_cmd_rc == 1 && $off < $file_sz
			if $off + $buf_sz > $file_sz
				set $buf_sz = $file_sz - $off
			end

			printf " 0x%x %d\n", $fl_off + $off, $buf_sz

			set fl_cmd_flash_addr = $fl_off + $off
			set fl_cmd_amount = $buf_sz

			# dump command
			set fl_cmd = 6

			# wait for flash dump to complete
			while fl_cmd != 0

			end
			if fl_cmd_rc == 1
				dump binary memory /tmp/foo.gdb fl_cmd_data fl_cmd_data + $buf_sz
				shell cat /tmp/foo.gdb >> $arg0
				shell rm /tmp/foo.gdb
			end
			set $off = $off + $buf_sz
		end
		if fl_cmd_rc == 1
			printf "Done\n"
		else
			printf "Error during flash dump %d\n", fl_cmd_rc
		end
	end
end

document fl_dump
usage: fl_dump <file> <id> <offset> <amount>
Asks flash_loader to dump contents of flash to a file.
  file    - name of the file to compare
  id     - flash identifier as specified for this BSP
  offset - offset to this flash
  amount - number of bytes to dump
end
