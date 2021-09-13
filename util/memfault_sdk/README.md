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
To use mynewt-memfault-sdk add following lines to **project.yml**.

```yaml
repository.mynewt-memfault-sdk:
    type: github
    vers: 0.0.0
    user: proxyco
    repo: mynewt-memfault-sdk
```

This dependency will allow you to pull in memfault features
```yaml
pkg.deps:
    - "@mynewt-memfault-sdk/memfault-firmware-sdk/ports/mynewt"
```

To use mynewt-memfault-sdk, set this values in **syscfg.vals:** section
```yaml
    MEMFAULT_ENABLE: 1
```

Other syscfgs you can configure are:
```yaml
    MEMFAULT_DEVICE_INFO_SOFTWARE_TYPE:
        description: A name to represent the firmware running on the MCU.
        value: '"app-fw"'
    
    MEMFAULT_DEVICE_INFO_SOFTWARE_VERS:
        description: The version of the "software_type" currently running.
        value: '"1.0.0"'
    
    MEMFAULT_DEVICE_INFO_HARDWARE_VERS:
        description: The revision of hardware for the device. This value must remain the same for a unique device.
        value: '"dvt1"'

    MEMFAULT_EVENT_STORAGE_SIZE:
        description: Storage size for Memfault events
        value: 1024
    
    MEMFAULT_SANITIZE_START:
        description: Start address of address sanitization range
        value: 0x00000000
        restrictions:
            - $notnull
    
    MEMFAULT_SANITIZE_SIZE:
        description: Size of address sanitization range
        value: 0xFFFFFFFF
        restrictions:
            - $notnull
    
    MEMFAULT_DEBUG_LOG_BUFFER_SIZE:
        description: Size of debug log buffer in bytes
        value: 128
    
    MEMFAULT_MEM_NZ_AREA:
        description: Memory area that does not get zeroed out at init time
        value: '".mflt.core.nz"'
```