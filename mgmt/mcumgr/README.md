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
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
-->

# mcumgr

mcumgr is the device management library of Apache Mynewt. It implements the
device side (server) of SMP (Simple Management Protocol) and provides
handlers for core management commands: image management, file system
management, log management, statistics and OS management.

mcumgr was developed in the separate `apache/mynewt-mcumgr` repository (based
on Mynewt newtmgr code) with an OS abstraction layer so that it could also be
used by other operating systems. It is now part of Apache Mynewt core again and
uses Mynewt APIs directly.

## Architecture

The mcumgr stack has the following layout:

```
+---------------------+---------------------+
|             <command handlers>            |
+---------------------+---------------------+
|                   mgmt                    |
+---------------------+---------------------+
|           <transfer encoding(s)>          |
+---------------------+---------------------+
|               <transport(s)>              |
+---------------------+---------------------+
```

* *Command handler*: Processes incoming mcumgr requests and generates
  corresponding responses. A command handler is associated with a single
  command type, defined by a (group ID, command ID) pair.
* *mgmt*: The core of mcumgr; facilitates the passing of requests and responses
  between the command handlers and the transports and transfer encodings.
* *Transfer encoding*: Defines how mcumgr requests and responses are encoded on
  the wire: SMP or OMP (SMP over CoAP).
* *Transport*: Sends and receives mcumgr packets over a particular medium.

## Packages

- `mgmt/mcumgr/mgmt`: management core; group registration and dispatch.
- `mgmt/mcumgr/smp`: Simple Management Protocol transfer encoding.
- `mgmt/mcumgr/omp`: OIC Management Protocol (SMP over CoAP) transfer
  encoding, used by `mgmt/oicmgr`.
- `mgmt/mcumgr/cmd/img_mgmt`: image management command handlers.
- `mgmt/mcumgr/cmd/fs_mgmt`: file system command handlers.
- `mgmt/mcumgr/cmd/log_mgmt`: log management command handlers.
- `mgmt/mcumgr/cmd/os_mgmt`: OS management command handlers.
- `mgmt/mcumgr/cmd/stat_mgmt`: statistics command handlers.
- `mgmt/mcumgr/util`: utility functions.
- `encoding/cborattr`: used for parsing incoming mcumgr requests.

SMP transports are provided by `mgmt/smp/transport/ble`,
`mgmt/smp/transport/smp_shell` and `mgmt/smp/transport/smp_uart`. Other
packages provide additional command groups, e.g. `sys/config` (config
management), `sys/shell` (shell command execution) and `mgmt/imgmgr`.

## Configuration

To use mcumgr, an application needs to depend on an SMP transport and on the
command handlers it wants to expose, e.g.:

```
    - '@apache-mynewt-core/mgmt/smp/transport/ble'
    - '@apache-mynewt-core/mgmt/smp/transport/smp_shell'
    - '@apache-mynewt-core/mgmt/mcumgr/cmd/fs_mgmt'
    - '@apache-mynewt-core/mgmt/mcumgr/cmd/img_mgmt'
    - '@apache-mynewt-core/mgmt/mcumgr/cmd/os_mgmt'
    - '@apache-mynewt-core/mgmt/mcumgr/smp'
```

For an example application see `apps/smp_svr` (SMP) and `apps/omp_svr` (OMP).

Image management requires the MCUboot boot loader.

## Documentation

- [SMP protocol](docs/protocol.md)
- [SMP over console](docs/smp-console.md)
- [SMP over Bluetooth](docs/smp-bluetooth.md)
- [Security threat model](THREAT_MODEL.md)

## Command line tool

The `mcumgr` command line tool is available at
https://github.com/apache/mynewt-mcumgr-cli. `newtmgr`
(https://github.com/apache/mynewt-newtmgr) can be used as well.
