<!---
#
#Licensed to the Apache Software Foundation (ASF) under one
#or more contributor license agreements.  See the NOTICE file
#distributed with this work for additional information
#regarding copyright ownership.  The ASF licenses this file
#to you under the Apache License, Version 2.0 (the
#"License"); you may not use this file except in compliance
#with the License.  You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
#Unless required by applicable law or agreed to in writing,
#software distributed under the License is distributed on an
#"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#KIND, either express or implied.  See the License for the
#specific language governing permissions and limitations
#under the License.
#
--->

# Infineon DPS3xx Digital Baromteric Pressure Sensor
Software Driver Package v1.0 for Apache Mynewt OS
<br>(c) Infineon Technologies AG
<hr>

#### Quick Start Guide

__1) Features:__<br><br>
	&emsp;&emsp;&emsp;- Supports polling based sensor data reporting <br>
	&emsp;&emsp;&emsp;- Supports I2C (address:0x77 or 0x76) and SPI(4 Wire) <br>
	&emsp;&emsp;&emsp;- Can be used as lib. driver for all DPS3xx family (DPS368/310 etc.) <br>
	
__2) Package Contents:__<br><br>
	&emsp;&emsp;&emsp;- 1) Driver sources located under dps368/src <br>	
	&emsp;&emsp;&emsp;- 2) Driver public interfaces and data structures header located under dps368/include/dps368 <br>
	&emsp;&emsp;&emsp;- 3) Driver package dependencies located under and as dps368/pkg.yml <br>
	&emsp;&emsp;&emsp;- 4) Driver package configuration parameters defined under and as dps368/syscfg.yml <br>
	
__3) How to Configure:__<br><br>

In order to integrate and configure the off board sensor DPS368 to target, please set I2C\_0 =1 DPS368\_OFB=1 and DPS368\_CLI=1 in syscfg of target.<br>

To configure sensor through application, we need to understand the meaning of members of **dps368_cfg_s** structure. Please refer to the excerpt of code below with comments explaing every field.<br>

```c

struct dps368_cfg_s {
	/*to set desired oversampling rate for temperature type*/
    dps3xx_osr_e osr_t; 
    
   /*to set desired oversampling rate for pressure type*/
    dps3xx_osr_e osr_p;
    
   /*to set desired ouptput data rate for temperature type*/
    dps3xx_odr_e odr_t;
    
   /*to set desired ouptput data rate for pressure type*/
    dps3xx_odr_e odr_p;
    
   /*to set desired opearation/power mode on sensor*/
    dps3xx_operating_modes_e mode;
    
   /*to set configure sensor for one or more desired options
   *Please refer to dps3xx_config_options_e section for details*/
    dps3xx_config_options_e config_opt;
   
	/*subtype of supported sensor data types to which the 
	*cfg is desired*/
    sensor_type_t chosen_type;
};

```
Following code example shows the configuration chosen at very first time when board is booted and sensor is configured with its default configuration (present in syscfg.yml). Here sensor initialization sequence together with configuration of OSR, ODR and mode is set using config\_opt flags.

```c
static int
config_dps368_sensor(void)
{
    int rc;
    struct os_dev *dev;
    struct dps368_cfg_s cfg;

	/*Set the configuration options using config_opt flags*
	 *Here very first time sensor init sequence is carried out
	 *togther with setting all configuration to its default values
    cfg.config_opt = DPS3xx_CONF_WITH_INIT_SEQUENCE | DPS3xx_RECONF_ALL;
    
    /*Default mode of operation can be mentioned in syscfg.yml*
     *Currently set to BACKGROUD_ALL for continuously streaming 
     *both pressure and temperature data*/
    cfg.mode = (dps3xx_operating_modes_e)MYNEWT_VAL(DPS368_DFLT_CONF_MODE);
    
    /*Set OSR and ODR for both pressure and temperature type
     *Please refer to datasheet for details*/
    cfg.odr_p = (dps3xx_odr_e) ( MYNEWT_VAL(DPS368_DFLT_CONF_ODR_P) << 4);
    cfg.odr_t = (dps3xx_odr_e) ( MYNEWT_VAL(DPS368_DFLT_CONF_ODR_T) << 4);
    cfg.osr_p = (dps3xx_osr_e)   MYNEWT_VAL(DPS368_DFLT_CONF_OSR_P);
    cfg.osr_t = (dps3xx_osr_e)   MYNEWT_VAL(DPS368_DFLT_CONF_OSR_T);

	/*Finally choose a data type from supported sensor data types to which
	*given configuration is desired to */
    cfg.chosen_type = SENSOR_TYPE_PRESSURE | SENSOR_TYPE_TEMPERATURE;

    dev = (struct os_dev *) os_dev_open("dps368_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

	/*Finally call to dps368_config will assert the configuration to actuall sensor*/
    rc = dps368_config((struct dps368 *) dev, &cfg);

    os_dev_close(dev);
    return rc;
}	

```
Similarly, in order to reconfigure only mode of operation on sensor appropriate configuration flags can be used. For example,

```c
static int
config_dps368_sensor(void)
{
    int rc;
    struct os_dev *dev;
    struct dps368_cfg_s cfg;

	/*Set the configuration options using config_opt flags*
	 *Now just indicate that only mode needs to be changed*/
    cfg.config_opt = DPS3xx_RECONF_MODE_ONLY;
    
    /*Set desired mode
    Here the sensor is put to idle/power down mode*/
    cfg.mode = DPS3xx_MODE_IDLE;
      
    dev = (struct os_dev *) os_dev_open("dps368_0", OS_TIMEOUT_NEVER, NULL);
    assert(dev != NULL);

	/*Finally call to dps368_config will assert the configuration to actuall sensor*/
    rc = dps368_config((struct dps368 *) dev, &cfg);

    os_dev_close(dev);
    return rc;
}	

```
This gives application flexibility to only configure the desired part of configuration, avoiding unnecessary transactions over bus.<br>After setting desired configuration and before polling sensor for data,__make sure that sensor is either in "Background" (continuous streaming) or "Command" (one shot) mode__  
Sensor data now can be polled through sensor\_read() or sensor\_manager/Listener etc.

