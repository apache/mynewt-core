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

<img src="http://mynewt.apache.org/img/logo.svg" width="250" alt="Apache Mynewt">

## Overview

Apache Mynewt is an open-source operating system for tiny embedded devices.
Its goal is to make it easy to develop applications for microcontroller
environments where power and cost are driving factors.

It currently supports the following hardware platforms:

* nRF52 DK from Nordic Semiconductor (Cortex-M4)
* RuuviTag Sensor beacon platform (Nordic nRF52832 based)
* nRF51 DK from Nordic Semiconductor (Cortex-M0)
* VBLUno51 from VNG IoT Lab (Nordic nRF51822 SoC based)
* BLE Nano from RedBear (Nordic nRF51822 SoC based)
* BLE Nano2 and Blend2 from RedBear (Nordic nRF52832 SoC based)
* BMD-300-EVAL-ES from Rigado (Cortex-M4)
* BMD-200 from Rigado (Cortex-M0)
* STM32F4DISCOVERY from ST Micro (Cortex-M4)
* STM32-E407 from Olimex (Cortex-M4)
* Arduino Zero (Cortex-M0)
* Arduino Zero Pro (Cortex-M0)
* Arduino M0 Pro (Cortex-M0)
* Arduino MKR1000 (Cortex-M0)
* Arduino Primo NRF52 (Cortex-M4)
* NUCLEO-F401RE (Cortex-M4)
* FRDM-K64F from NXP (Cortex-M4)
* BBC micro:bit (Nordic nrf51822; Cortex-M0)

Apache Mynewt uses the
[Newt](https://www.github.com/apache/incubator-mynewt-newt) build and package
management system, which allows you to compose your OS and choose only the
components you need.

This repository contains the core packages of the Apache Mynewt OS, including:

* A Pre-emptive, Real Time OS Kernel
* A open-source Bluetooth 4.2 stack (both Host & Controller), NimBLE, that
completely replaces the proprietary SoftDevice on Nordic chipsets.
    - Support for 251 byte packet size
    - Support for all 4 roles concurrently - Broadcaster, Observer, Peripheral and Central
    - Support for up to 32 simultaneous connections.
    - Legacy and SC (secure connections) SMP support (pairing and bonding).
* A flash filesystem, NFFS, which is designed for tiny (128KB->16MB) flashes.
* FatFS
* Flash Circular Buffer
* JSON and CBOR encoding
* Bootloader support
* Remote Software Upgrade
* HAL and BSP infrastructure designed to abstract microcontroller specifics
* Shell and Console support
* Statistics and Logging Infrastructure
* OIC Client and Server

For more information on the Mynewt OS, please visit our website [here](https://mynewt.apache.org/).
If you'd like to get started, visit the [Quick Start Guide](http://mynewt.apache.org/quick-start/).

## Roadmap

Mynewt is being actively developed.  Some of the features we're currently working on:

* Deep sleep: Mynewt OS will coordinate peripherals and processor to put the
  processor to sleep when all tasks are idle.
* Sensor API
* IP Stack
* Support for different boards and microcontroller architectures, we're working
  on:
    - ESP8266
    - Arduino Uno
    - Arduino Mega

## Browsing

If you are browsing around the source tree, and want to see some of the
major functional chunks, here are a few pointers:

- kernel: Contains the core of the RTOS ([kernel/os](https://github.com/apache/incubator-mynewt-core/tree/master/kernel/os))

- sys: Contains a number of helper libraries for building applications.  Including a
console ([sys/console](https://github.com/apache/incubator-mynewt-core/tree/master/sys/console))),
shell ([sys/shell](https://github.com/apache/incubator-mynewt-core/tree/master/sys/shell)))

- mgmt: Contains the management libraries for newtmgr [mgmt/newtmgr](https://github.com/apache/incubator-mynewt-core/tree/master/sys/newtmgr)), which supports software upgrade and remote fetching of logs and statistics.

- net: Contains the networking packages.  Highlights of the net directory are the NimBLE and IP packages.
[Nimble](https://github.com/apache/incubator-mynewt-core/tree/master/net/nimble)
is a full Bluetooth host and controller implementation, that is written
from the ground up for the Apache Mynewt Operating System.
[ip](https://github.com/apache/incubator-mynewt-core/tree/master/net/ip) is a port of LWIP, a complete IPv4 and IPv6 implementation.

- hw: Contains the HW specific support packages.  Board Support Packages
are located in [hw/bsp](https://github.com/apache/incubator-mynewt-core/tree/master/hw/bsp),
and the MCU specific definitions they rely on are located in
[hw/mcu](https://github.com/apache/incubator-mynewt-core/tree/master/hw/mcu).
There is a HAL (Hardware Abstraction Layer) stored in
[hw/hal](https://github.com/apache/incubator-mynewt-core/tree/master/hw/hal), even
though the implementation of various HALs are stored in the MCU specific definitions.  Finally, drivers can be found in [hw/drivers](https://github.com/apache/incubator-mynewt-core/tree/master/hw/drivers).  Drivers provide a higher-level interface to the hardware than the HAL, and may require the Mynewt operating system to function.

- fs: Contains the FS package ([fs/fs](https://github.com/apache/incubator-mynewt-core/tree/master/fs/fs))
which is the high-level Apache Mynewt file system API.   A specific implementation of that FS, is
[NFFS](https://github.com/apache/incubator-mynewt-core/tree/master/fs/nffs) (Newtron
Flash File System.)  The Newtron file system is a FS that has been built from
the ground-up in Apache Mynewt, designed to be optimized for small
(64KB-32MB) flashes.
The fs directory also contains [fcb](http://mynewt.apache.org/latest/os/modules/fcb/fcb/), a flash circular buffer implementation.

## Sample Applications

In addition to some of the core packages, there are also some sample
applications that show how to instantiate the Apache Mynewt system.  These
sample applications are located in the `apps/` directory.  They include:

* [boot](https://github.com/apache/incubator-mynewt-core/tree/master/apps/boot):
  Project to build the bootloader for test platforms.
* [blinky](https://github.com/apache/incubator-mynewt-core/tree/master/apps/blinky): The
  minimal packages to build the OS, and blink a LED!
* [slinky](https://github.com/apache/incubator-mynewt-core/tree/master/apps/slinky): A
  slightly more complex project that includes the console and shell libraries.
* [blecent](https://github.com/apache/incubator-mynewt-core/tree/master/apps/blecent): A basic central device with no user interface.  This
application scans for a peripheral that supports the alert notification
service (ANS).  Upon discovering such a peripheral, blecent connects and
performs a characteristic read, characteristic write, and notification subscription.
* [blehci](https://github.com/apache/incubator-mynewt-core/tree/master/apps/blehci): Implements a BLE controller-only application.  A separate
host-only implementation, such as Linux's BlueZ, can interface with this
application via HCI over UART.
* [bleprph](https://github.com/apache/incubator-mynewt-core/tree/master/apps/bleprph): An
  implementation of a minimal BLE peripheral.
* [bletiny](https://github.com/apache/incubator-mynewt-core/tree/master/apps/bletiny): A
  stripped down interface to the Apache Mynewt Bluetooth stack.
* [bleuart](https://github.com/apache/incubator-mynewt-core/tree/master/apps/bleuart):
Implements a simple BLE peripheral that supports the Nordic
UART / Serial Port Emulation service
(https://developer.nordicsemi.com/nRF5_SDK/nRF51_SDK_v8.x.x/doc/8.0.0/s110/html/a00072.html).
* [test](https://github.com/apache/incubator-mynewt-core/tree/master/apps/test): Test
  project which can be compiled either with the simulator, or on a per-architecture basis.
  Test will run all the package's unit tests.

# Getting Help

If you are having trouble using or contributing to Apache Mynewt, or just want to talk
to a human about what you're working on, you can contact us via the
[developers mailing list](mailto:dev@mynewt.incubator.apache.org).

Although not a formal channel, you can also find a number of core developers
on the #mynewt channel on Freenode.

Also, be sure to checkout the [Frequently Asked Questions](https://mynewt.apache.org/faq/answers)
for some help troubleshooting first.

# Contributing

Anybody who works with Apache Mynewt can be a contributing member of the
community that develops and deploys it.  The process of releasing an operating
system for microcontrollers is never done: and we welcome your contributions
to that effort.

More information can be found at the Community section of the Apache Mynewt
website, located [here](https://mynewt.apache.org/community).

## Pull Requests

Apache Mynewt welcomes pull request via Github.  Discussions are done on Github,
but depending on the topic, can also be relayed to the official Apache Mynewt
developer mailing list dev@mynewt.incubator.apache.org.

If you are suggesting a new feature, please email the developer list directly,
with a description of the feature you are planning to work on.

We do not merge pull requests directly on Github, all PRs will be pulled and
pushed through https://git.apache.org/.

## Filing Bugs

Bugs can be filed on the
[Apache Mynewt Bug Tracker](https://issues.apache.org/jira/browse/MYNEWT).

Where possible, please include a self-contained reproduction case!

## Feature Requests

Feature requests should also be filed on the
[Apache Mynewt Bug Tracker](https://issues.apache.org/jira/browse/MYNEWT).
Please introduce it as a ticket type "Wish."

## Writing Tests

We love getting newt tests!  Apache Mynewt is a huge undertaking, and improving
code coverage is a win for every Apache Mynewt user.

## Writing Documentation

Contributing to documentation (in addition to writing tests), is a great way
to get involved with the Apache Mynewt project.

Pull requests to the apache-mynewt-site repository on Github are the best
way to submit documentation.  For more information on contributing to the
documentation, the [FAQ](https://mynewt.apache.org/faq/answers/) has some
more information.

# License

The code in this repository is all under either the Apache 2 license, or a
license compatible with the Apache 2 license.  See the LICENSE file for more
information.

# Export restrictions

This distribution includes cryptographic software. The country in which you
currently reside may have restrictions on the import, possession, use, and/or
re-export to another country, of encryption software. BEFORE using any encryption
software, please check your country's laws, regulations and policies concerning
the import, possession, or use, and re-export of encryption software, to see if
this is permitted. See <http://www.wassenaar.org/> for more information.

The U.S. Government Department of Commerce, Bureau of Industry and Security (BIS),
has classified this software as Export Commodity Control Number (ECCN) 5D002.C.1,
which includes information security software using or performing cryptographic
functions with asymmetric algorithms. The form and manner of this Apache Software
Foundation distribution makes it eligible for export under the License Exception ENC
Technology Software Unrestricted (TSU) exception (see the BIS Export Administration
Regulations, Section 740.13) for both object code and source code.

The following provides more details on the included cryptographic software:
https://tls.mbed.org/supported-ssl-ciphersuites.
