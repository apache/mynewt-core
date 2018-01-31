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
#include "sensor/sensor.h"

struct bq24040_cfg {
	int pg_pin_num;
	int chg_pin_num;
	int ts_pin_num;
	int iset2_pin_num;
	sensor_type_t s_mask;
};

struct bq24040_int {
	os_sr_t lock;
	struct os_sem wait;
	bool active;
	bool sleep;
};

enum bq24040_int_pin {
	BQ24040_INT_PIN_PG 	=	0,
	BQ24040_INT_PIN_CHG	=	1,
	BQ24040_INT_PIN_MAX	=	2,
};

struct bq24040 {
	struct os_dev 			dev;
	struct sensor 			sensor;
	struct bq24040_cfg 		cfg;
	os_time_t 				last_read_time;
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
