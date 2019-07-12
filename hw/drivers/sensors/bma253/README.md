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



/*********************************************************/
driver for bma253
version:1.0.0.4  2019/7/12 
/*********************************************************/
target platform: nrf52832
BSP: nrf52dk
/*********************************************************/
mynewt-core version: 0-dev
mynewt-nimble version: 0-dev
/*********************************************************/
test tool: apps/sensors_test
           bma253_shell

[command for testing double tap feature]
- option 1:
sensor notify bma253_0 on double
or
- option 2:
bma253 wait-for-tap double

if using the sensor_shell (option 1), the double tap could be turned off by:
sensor notify bma253_0 off double


[command for testing low_g feature]
- option 1:
sensor notify bma253_0 on freefall


if using the sensor_shell (option 1), the low_g could be turned off by:
sensor notify bma253_0 off freefall

[command for testing orient change feature]
- option 1:
sensor notify bma253_0 on orient


if using the sensor_shell (option 1), the orient change could be turned off by:
sensor notify bma253_0 off orient

[command for testing sleep feature]
- option 1:
sensor notify bma253_0 on sleep


if using the sensor_shell (option 1), the sleep could be turned off by:
sensor notify bma253_0 off sleep

[command for testing wakeup feature]
- option 1:
sensor notify bma253_0 on wakeup


if using the sensor_shell (option 1), the wakeup could be turned off by:
sensor notify bma253_0 off wakeup


[command for testing orient feature]
- option 1:
sensor notify bma253_0 on orient_xl
other options: orient_yl, orient_zl, orient_xh, orient_yh, orient_zh

if using the sensor_shell (option 1), the orient could be turned off by:
sensor notify bma253_0 off orient_xl
other options: orient_yl, orient_zl, orient_xh, orient_yh, orient_zh

[command for sensor data read test]
sensor read bmp253_0 1 -n 10 -i 20 -d 4000
    for sensor read, both "polling" and "streaming" modes are supported
    [polling mode - BMA253_READ_M_POLL]
    the polling interval is specified by the "-i" argument to the "sensor" commmand, and
    polling duration is specified by "-d" argument, and both are in "mill-seconds"

    [streaming mode - BMA253_READ_M_STREAM]
    In this mode, currently, the driver enters an loop until the "timeout" parameter (currently is INT_MAX requested from sensor_shell)
    is reached and keeps reading from sensor FIFO, and events are reported to its sensor data listeners.

    The way the driver is implemented may be changed in future if sensor framework split read request in to two API calls:
    setup_read(this is where sensors are set up for subsequenet polling or interrupt based streaming), and poll()/report_data().


Any feedback is welcome, please write to our support contact for any questions or suggestions.

