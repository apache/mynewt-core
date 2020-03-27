/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
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

#ifndef _HW_DRIVERS_I2S_DRIVER_H
#define _HW_DRIVERS_I2S_DRIVER_H

struct i2s;
struct i2s_cfg;
struct i2s_sample_buffer;

/* Functions to be implemented by driver */

int i2s_create(struct i2s *i2s, const char *name, const struct i2s_cfg *cfg);

int i2s_driver_start(struct i2s *i2s);
int i2s_driver_stop(struct i2s *i2s);
void i2s_driver_buffer_queued(struct i2s *i2s);
int i2s_driver_suspend(struct i2s *i2s, os_time_t timeout, int arg);
int i2s_driver_resume(struct i2s *i2s);

/* Functions to be called by driver code */
int i2s_init(struct i2s *i2s, struct i2s_buffer_pool *pool);
void i2s_driver_state_changed(struct i2s *i2s, enum i2s_state);
void i2s_driver_buffer_put(struct i2s *i2s, struct i2s_sample_buffer *buffer);
struct i2s_sample_buffer *i2s_driver_buffer_get(struct i2s *i2s);

#endif /* _HW_DRIVERS_I2S_DRIVER_H */
