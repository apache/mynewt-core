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

To use the memfault-firmware-sdk with mynewt add following lines to
**project.yml**.

```yaml
repository.memfault-firmware-sdk:
  type: github
  vers: 0.0.0
  user: memfault
  repo: memfault-firmware-sdk
```

Adding this dependency will allow you to pull in Memfault features

```yaml
pkg.deps:
  - "@memfault-firmware-sdk/ports/mynewt"
```

You will also need to enable the following options in the **syscfg.vals:**
section of your `syscfg.yml` file:

```yaml
syscfg.vals:
    [...]
    MEMFAULT_ASSERT_CB: 1
    MEMFAULT_COREDUMP_CB: 1
    MEMFAULT_ENABLE: 1
    OS_COREDUMP: 1
    OS_COREDUMP_CB: 1
    OS_ASSERT_CB: 1
```

Other syscfgs you can configure can be found in [syscfg.yml](syscfg.yml)

Finally, on boot up initialize the Memfault subsystem from your `main` routine:

```c
#include "memfault/components.h"
int
main(int argc, char **argv)
{
// ...
    memfault_platform_boot();
// ...
}
```

## Using the GNU Build Id for Indexing

We recommend enabling the generation of a unique identifier for your project.
See https://mflt.io/symbol-file-build-ids for more details.

To enable, add the following compilation flags to your `pkg.yml`:

```
pkg.lflags:
  - -Wl,--build-id
pkg.cflags:
  - -DMEMFAULT_USE_GNU_BUILD_ID=1
```

And add the following **after** the `.text` in your targets linker script:

```
    .note.gnu.build-id :
    {
        __start_gnu_build_id_start = .;
        KEEP(*(.note.gnu.build-id))
    } > FLASH
```