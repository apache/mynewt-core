<!--
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#  KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
-->

## Overview

This is an app which works in concert with out-of-device program
to populate device flash. Program assumes programmer can directly write
to program memory. App polls for incoming commands (load, erase etc).

You can use this together with gdb user-defined functions in
compiler/gdbmacros/flash_loader.gdb. Or you can write your own.

### Example of how to use this with scripts

Here's a target for nrf52dk, app is built as RAM resident.
```
[marko@IsMyLaptop:~/src/incubator-mynewt-blinky]$ newt target show fl_nrf52dk
targets/fl_nrf52dk
    app=@apache-mynewt-core/apps/flash_loader
    bsp=@apache-mynewt-core/hw/bsp/nordic_pca10040
    build_profile=optimized
    syscfg=FLASH_LOADER_DL_SZ=0x8000:RAM_RESIDENT=1
```
And then have this as the gdb script file
```
[marko@IsMyLaptop:~/src/incubator-mynewt-blinky]$ cat loader_nrf52.gdb
set pagination off

shell sh -c "trap '' 2; JLinkGDBServer -device nRF52 -speed 4000 -if SWD -port 3333 -singlerun > logfile.txt 2>&1 &"
target remote localhost:3333
mon reset
restore bin/targets/fl_nrf52dk/app/apps/flash_loader/flash_loader.elf.bin binary 0x20000000
symbol-file bin/targets/fl_nrf52dk/app/apps/flash_loader/flash_loader.elf
set $sp=*(int *)0x20000000
set $pc=*(int *)0x20000004
b blink_led
c

source repos/apache-mynewt-core/compiler/gdbmacros/flash_loader.gdb
mon go

fl_load nrf52.img 0 0x42000
mon reset
quit
```

and then to run it:
```
arm-elf-linux-gdb -x loader_nrf52.gdb
```

