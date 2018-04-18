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

## Table of Contents
- [Overview](#overview)
- [Usage](#usage)
- [Wrap-up](#wrap)

## Overview<a name=overview></a>

The goal of the mynewt BMA2XX sensor driver is to allow a common interface 
across the family of [Bosch BMA200 series accelerometers](https://www.bosch-sensortec.com/bst/products/motion/accelerometers/overview_accelerometers).
Fortunately, the line of sensors have very similar functionality, and share 
the same registers mappings and interrupt engine.  The main differences are 
that each sensor has different bit-depth (14, 12, 10bit) and chip ids. 

The BMA2XX is based off the functionality of the mynewt BMA253 driver, but 
now includes SPI and I2C support and will support the following models:

* BMA280 - Supported
* BMA255 - (Coming soon)
* BMA253 - Supported
* BMA250E - (Coming soon)
* BMA22E - (Coming soon)
* BMA220 - (Coming soon)


Bosch-Sensortech has also published a [generic driver](https://github.com/BoschSensortec/BMA2x2_driver)
for reference 

## Usage<a name=usage></a>

The BMA2XX driver is designed as an (offboard sensor) and is instantiated from
 the sensor creator.  It also has shell support and defines a "bma2xx" command 
 option.  To test it out let's walk through a simple sensor_test application.
 
NOTE:  Stay tuned for 
 
### Create Target
First let's create a new target and base it off the sensor_test app.  We will
 also enable the shell, the driver and indicate if we are SPI or I2C.
~~~~
> newt target create bma2xx_sensor_test
Target targets/bma2xx_sensor_test successfully created 
~~~~
~~~~
> newt target set bma2xx_sensor_test app=@apache-mynewt-core/apps/sensors_test bsp=@apache-mynewt-core/hw/bsp/bmd300eval build_profile=debug
Target targets/bma2xx_sensor_test successfully set target.app to @apache-mynewt-core/apps/sensors_test
Target targets/bma2xx_sensor_test successfully set target.bsp to @apache-mynewt-core/hw/bsp/bmd300eval
Target targets/bma2xx_sensor_test successfully set target.build_profile to debug
~~~~
~~~~
> newt target set bma2xx_sensor_test syscfg=BMA2XX_OFB=1:SPI_0_MASTER=1:BMA2XX_CLI=1:SHELL_TASK=1
Target targets/bma2xx_sensor_test successfully set target.syscfg to BMA2XX_OFB=1:SPI_0_MASTER=1:BMA2XX_CLI=1:SHELL_TASK=1
~~~~

### Set Model in Config
As mentioned earlier the BMA2XX driver is supported in the sensor creator, 
and comes with a set of default settings.  The key attribute that needs to be
 set is the specific model (eg. BMA280, BMA253, etc) so that chip_ids and 
 resolution of accelerometer values are correct.  This is found in 
 [sensor_creator.c](https://github.com/apache/mynewt-core/blob/master/hw/sensor/creator/src/sensor_creator.c)  
in the config_bma2xx_sensor() function, and we will indicate that the BMA280 
is present.  
~~~~
int
config_bma2xx_sensor(void)
{
    struct os_dev * dev;
    struct bma2xx_cfg cfg;
    int rc;

    dev = os_dev_open("bma2xx_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

    // set the model attribute to one of the following enums
    // enum bma2xx_model {
    //   BMA2XX_BMA280 = 0,
    //   BMA2XX_BMA253 = 1,
    // };
    //
    cfg.model = BMA2XX_BMA280;
    
    
    .
    .
    .
}
~~~~

### Set Pins
Don't forgot to update your SPI/I2C pins the appropriate BSP configs and 
sensor_creator.c file.  We will not cover how to do that here. 

### Load App
Now it's time to build, and load the app to the target bsp.  We will assume 
the bootloader is already built and installed.  See [this tutorial](https://mynewt.apache.org/latest/os/tutorials/sensors/sensor_thingy_lis2dh12_onb/) 
for more instructions.
Using the "newt run" command (build, load, gdb) we get the following:

~~~~
> newt run bma2xx_sensor_test 0.0.0.1
    .
    .
    .
at repos/apache-mynewt-core/hw/mcu/nordic/nrf52xxx/src/hal_os_tick.c:165
165	    if (ticks > 0) {
Resetting target
0x000000dc in ?? ()
(gdb) c
Continuing.
~~~~


### Read Data
Now that the app is loaded and running in a gdb session let's interact with the 
shell to read some data. Below is output from uart session with the BSP. 
Please note the command available under the "bma2xx" command option.
~~~~
help

009897 help
009897 stat                          
009898 config                        
009899 log                           
009900 flash                         
009901 i2c_scan                      
009902 tasks                         
009903 mpool                         
009904 date                          
009905 sensor                        
009906 bma2xx                        
009907                               
009908 compat> bma2xx

010864 self-test <default|strict>
010864 offset-compensation <x={0g|-1g|+1g}> <y={0g|-1g|+1g}> <z={0g|-1g|+1g}>
010866 query-offsets 
010867 write-offsets 
010868 stream-read <num-reads>
010868 current-temp 
010869 current-orient 
010870 wait-for-orient 
010870 wait-for-high-g 
010871 wait-for-low-g 
010872 wait-for-tap <double|single>
010872 power-settings <normal|deep-suspend|suspend|standby|lpm1|lpm2>
                      <0.5ms|1ms|2ms|4ms|6ms|10ms|25ms|50ms|100ms|500ms|1s>
010876 compat> bma2xx stream-read 10

012040 x = 0.009516000 y = 0.017324000 z = 1.004791975
012041 x = 0.011224000 y = 0.021715998 z = 0.998936000
012042 x = 0.012932000 y = 0.021472000 z = 0.997716032
012044 x = 0.019032000 y = 0.019763998 z = 1.001863956
012045 x = 0.016592000 y = 0.017811998 z = 0.996495936
012046 x = 0.017080000 y = 0.016348001 z = 1.001132011
012048 x = 0.016104000 y = 0.011224000 z = 0.993323968
012049 x = 0.017324000 y = 0.015372000 z = 1.006255984
012051 x = 0.015616000 y = 0.015616000 z = 0.994544000
012052 x = 0.014884000 y = 0.018056000 z = 0.999668032
012065 compat> 
~~~~

## Wrap-Up
As shown in the example above, in one package your app can support the family
 of Bosch BMA200 sensors with minimal configuration.  Also, using the "newt 
 size" command the package runs ~13k in debug mode  In the coming weeks the
  intent is complete support of the rest of the sensors, and provide more in 
  depth tutorials that exercise the sensor API. 
