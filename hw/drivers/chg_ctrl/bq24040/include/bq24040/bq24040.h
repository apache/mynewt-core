/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * resarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef __BQ24040_H__
#define __BQ24040_H__

#include "os/os.h"
#include "hal/hal_gpio.h"
#include "sensor/sensor.h"

typedef void (*bq24040_interrupt_handler)(void *arg);

struct bq24040_pin {
	int 						bp_pin_num;
	int 						bp_init_value;
	hal_gpio_irq_trig_t 		bp_trig;
	hal_gpio_irq_pull_t 		bp_pull;
	bq24040_interrupt_handler 	bp_int;
};

struct bq24040_cfg {
	struct bq24040_pin 			bc_pg_pin;
	struct bq24040_pin 			bc_chg_pin;
	struct bq24040_pin 			bc_ts_pin;
	struct bq24040_pin 			bc_iset2_pin;
	sensor_type_t 				bc_s_mask;
};

struct bq24040 {
	struct os_dev 				b_dev;
	struct sensor 				b_sensor;
	struct bq24040_cfg 			b_cfg;
	os_time_t 					b_last_read_time;
};

/**
 * Initialize the bq24040
 *
 * @param dev 	Pointer to the bq24040_dev device descriptor
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
bq24040_init(struct os_dev *dev, void *arg);

/**
 * Configure BQ24040 sensor
 *
 * @param Sensor device BQ24040 structure
 * @param Sensor device BQ24040 configuration structure
 *
 * @return 0 on success, non-zero on failure
 */
int
bq24040_config(struct bq24040 *, struct bq24040_cfg *);

/**
 * Reads the state of the charge source
 *
 * @param Sensor device BQ24040 structure
 * @param Charge source status to be returned by the function (0 if charge
 *     source is not detected, 1 if it is detected)
 *
 * @return 0 on success, non-zero on failure
 */
int
bq24040_get_charge_source_status(struct bq24040 *, int *);

/**
 * Reads the battery charging status
 *
 * @param Sensor device BQ24040 structure
 * @param Battery charging status returned by the function (0 if the battery is
 *     not charging, 1 if it is charging)
 *
 * @return 0 on success, non-zero on failure
 */
int
bq24040_get_charging_status(struct bq24040 *, int *);

/**
 * Set the BQ24040 TS pin to logic 1 (if configured), enabling charger mode on 
 * the IC
 *
 * @param Sensor device BQ24040 structure
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
bq24040_enable(struct bq24040 *);

/**
 * Clear the BQ24040 TS pin to logic 0 (if configured), disabling the IC
 *
 * @param Sensor device BQ24040 structure
 *
 * @return 0 on success, and non-zero error code on failure
 */
int
bq24040_disable(struct bq24040 *);

#endif
