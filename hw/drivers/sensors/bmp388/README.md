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
driver for bmp388
version: 1.0.2 2019/5/10 
/*********************************************************/
target platform: nrf52832
BSP: nrf52dk
/*********************************************************/
mynewt-core version: 0-dev
mynewt-nimble version: 0-dev
/*********************************************************/
test tool: apps/sensors_test
           bmp388_shell
cmd for interrupt feature test
sensor notify bmp388_0 on wakeup

cmd for sensor data read test
sensor read bmp388_0 0x40 -n 1 -i 100 
sensor read bmp388_0 0x20 -n 1 -i 100
sensor read bmp388_0 0x40 -n 100 -i 100
sensor read bmp388_0 0x20 -n 100 -i 100

cmd for bmp388 selftest
bmp388 test
cmd for bmp388 registers dumping
bmp388 dump
cmd for getting bmp388 chipid
bmp388 chipid


since we do not have very specific testing guideline / cases for the driver,
any feedback is welcome.
/*********************************************************/
surpported features:
sensor data poll read in force mode
sensor FIFO full interrupt
sensor FIFO water mark level interrupt
sensor FIFO
sensor data ready interrupt
sensor time in FIFO
sensor register dump: cmd->bmp388 dump
sensor chipid read: cmd->bmp388 chipid
sensor powermode change: cfg.power_mode
I2C and SPI are supportted.

please refer to pdf file interrupt_features_polling_settings,
I2C/SPI setting can also be found in this PDF.
this documentation will guide you on how to set and use below features.

sensor data poll read in force mode
sensor FIFO full interrupt
sensor FIFO water mark level interrupt
sensor FIFO
sensor data ready interrupt
sensor time in FIFO

