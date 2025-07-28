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

## MSC_FLASH

Package adds functionality to expose whole flash or one flash area as block device over USB MSC interface.
This allows to read and write to flash from host computer directly to flash. Device will not have file system provided by package so it's up to the system to utilize it.

### Configuration
Add package to **application** or **target** `pkg.deps:` list.

For example following configuration will present content of slot 1 flash as RAW disk. 
```yaml
syscfg.vals:
    MSC_FLASH_FLASH_AREA_ID: FLASH_AREA_IMAGE_1
```

To expose entire flash (not single flash area) following configuration could be used:
```yaml
syscfg.vals:
    MSC_FLASH_FLASH_ID: 0
```
If some starting flash sector should not be exposed (used by bootloader), starting offset can be provided. In following example first 16kB of flash will not be visible. 
```yaml
syscfg.vals:
    MSC_FLASH_FLASH_ID: 0
    MSC_FLASH_FLASH_OFFSET: 0x4000
```

Complete target settings to expose flash could look like this

**syscfg.yml**:
```yaml
syscfg.vals:
    USBD_VID: 0xABCD
    USBD_PID: 0x1234
    USBD_VENDOR_STRING: '"Vendor"'
    USBD_PRODUCT_STRING: '"FlashDisk"'
    MSC_FLASH_FLASH_AREA_ID: FLASH_AREA_IMAGE_1
```

**pkg.yml**:
```yaml
pkg.deps:
    - "@apache-mynewt-core/hw/usb/tinyusb/msc_flash"
```
