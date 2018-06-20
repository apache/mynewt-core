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

#include <stdint.h>

#ifndef _UTIL_EASING_H_
#define _UTIL_EASING_H_

typedef float (*easing_f_func_t)(float step, float max_steps, float max_val);
typedef int32_t (*easing_int_func_t)(int32_t step, int32_t max_steps, int32_t max_val);

/* Float Functions */

/* Custom */
float exponential_custom_f_io(float step, float max_steps, float max_val);
float exp_sin_custom_f_io(float step, float max_steps, float max_val);
float sine_custom_f_io(float step, float max_steps, float max_val);

/* Linear */
float linear_f_io(float step, float max_steps, float max_val);

/* Exponential */
float exponential_f_in(float step, float max_steps, float max_val);
float exponential_f_out(float step, float max_steps, float max_val);
float exponential_f_io(float step, float max_steps, float max_val);

/* Quadratic */
float quadratic_f_in(float step, float max_steps, float max_val);
float quadratic_f_out(float step, float max_steps, float max_val);
float quadratic_f_io(float step, float max_steps, float max_val);

/* Cubic */
float cubic_f_in(float step, float max_steps, float max_val);
float cubic_f_out(float step, float max_steps, float max_val);
float cubic_f_int_io(float step, float max_steps, float max_val);

/* Quartic */
float quartic_f_in(float step, float max_steps, float max_val);
float quartic_f_out(float step, float max_steps, float max_val);
float quartic_f_io(float step, float max_steps, float max_val);

/* Quintic */
float quintic_f_in(float step, float max_steps, float max_val);
float quintic_f_out(float step, float max_steps, float max_val);
float quintic_f_io(float step, float max_steps, float max_val);

/* Circular */
float circular_f_in(float step, float max_steps, float max_val);
float circular_f_out(float step, float max_steps, float max_val);
float circular_f_io(float step, float max_steps, float max_val);

/* Sine */
float sine_f_in(float step, float max_steps, float max_val);
float sine_f_out(float step, float max_steps, float max_val);
float sine_f_io(float step, float max_steps, float max_val);

/* Bounce */
float bounce_f_in(float step, float max_steps, float max_val);
float bounce_f_out(float step, float max_steps, float max_val);
float bounce_f_io(float step, float max_steps, float max_val);

/* Back */
float back_f_in(float step, float max_steps, float max_val);
float back_f_out(float step, float max_steps, float max_val);
float back_f_io(float step, float max_steps, float max_val);


/* Integer Functions */

/* Custom */
int32_t exponential_custom_int_io(int32_t step, int32_t max_steps, int32_t max_val);
int32_t exp_sin_custom_int_io(int32_t step, int32_t max_steps, int32_t max_val);
int32_t sine_custom_int_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Linear */
int32_t linear_int_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Exponential */
int32_t exponential_int_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t exponential_int_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t exponential_int_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Quadratic */
int32_t quadratic_int_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t quadratic_int_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t quadratic_int_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Cubic */
int32_t cubic_int_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t cubic_int_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t cubic_int_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Quartic */
int32_t quartic_int_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t quartic_int_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t quartic_int_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Qintic */
int32_t quintic_int_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t quintic_int_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t quintic_int_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Circular */
int32_t circular_int_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t circular_int_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t circular_int_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Sine */
int32_t sine_int_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t sine_int_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t sine_int_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Bounce */
int32_t bounce_int_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t bounce_int_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t bounce_int_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Back */
int32_t back_int_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t back_int_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t back_int_io(int32_t step, int32_t max_steps, int32_t max_val);

#endif /* _UTIL_EASING_H_ */
