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

#include <math.h>
#include "easing/easing.h"

#define HALF_PI 1.57079632679f   /* PI/2 */
#define ONE_DIV_E 0.36787944117f /* 1/e */
#define PI_TIMES2 6.28318530718f  /* 2PI */

/* Custom */
static inline float exponential_custom_io(float step, float max_steps, float max_val)
{
    float R = max_steps * log10(2) / log10(max_val);
    return pow(2, (step / R)) - 1;
}

static inline float exp_sin_custom_io(float step, float max_steps, float max_val)
{
    float mplier = max_val / 2.35040238729f;
    float pi_d_maxs = M_PI / max_steps;
    step += max_steps;
    return ( exp( sin((step * pi_d_maxs) + M_PI_2)) - ONE_DIV_E ) * mplier;
}

static inline float sine_custom_io(float step, float max_steps, float max_val)
{
    return max_val * cos((PI_TIMES2 * step/max_steps) + M_PI) + max_val;
}

/* Linear */
static inline float linear_io(float step, float max_steps, float max_val)
{
	return step * max_val / max_steps;
}

/* Exponential */
static inline float exponential_in(float step, float max_steps, float max_val)
{
    return (step == 0) ?
        0 :
        pow(max_val, (float)step/max_steps);
}

static inline float exponential_out(float step, float max_steps, float max_val)
{
	return (step == max_steps) ?
        max_val :
        max_val - pow(max_val, 1.0 - (float)step/max_steps);
}

static inline float exponential_io(float step, float max_steps, float max_val)
{
	float ratio = ( (float)step / (max_steps / 2.0) );

	if (step == 0)
		return 0;

	if (step == max_steps)
		return max_val;

	if (ratio < 1)
        return powf((float)max_val/2.0, (float)step/(max_steps/2.0));

	return max_val/2.0 + max_val/2.0 -
        pow(max_val/2.0, 1.0 - ((float)step - (max_steps / 2)) /
            (max_steps / 2) );
}

/* Quadratic */
static inline float quadratic_in(float step, float max_steps, float max_val)
{
	float ratio = step / max_steps;

	return max_val * pow(ratio, 2);
}

static inline float quadratic_out(float step, float max_steps, float max_val)
{
    float ratio = step / max_steps;

	return -max_val * ratio * (ratio - 2.0);
}

static inline float quadratic_io(float step, float max_steps, float max_val)
{
	float ratio = step / (max_steps / 2.0);

	if (ratio < 1)
		return max_val / 2.0 * pow(ratio, 2);

    ratio = (step - (max_steps/2.0)) / (max_steps/2.0);
    return (max_val / 2.0) - (max_val / 2.0) * ratio * (ratio - 2.0);
}

/* Cubic */
static inline float cubic_in(float step, float max_steps, float max_val)
{
	float ratio = step / max_steps;

	return max_val * pow(ratio, 3);
}

static inline float cubic_out(float step, float max_steps, float max_val)
{
	float ratio = step / (max_steps - 1.0);

	return max_val * (pow(ratio, 3) + 1);
}

static inline float cubic_io(float step, float max_steps, float max_val)
{
	float ratio = step / (max_steps / 2.0);

	if (ratio < 1)
		return max_val / 2 * pow(ratio, 3);

	return max_val / 2 * (pow(ratio - 2, 3) + 2);
}

/* Quartic */
static inline float quartic_in(float step, float max_steps, float max_val)
{
	float ratio = step / max_steps;

	return max_val * pow(ratio, 4);
}

static inline float quartic_out(float step, float max_steps, float max_val)
{
	float ratio = (step / max_steps) - 1.0;

	return max_val + max_val * pow(ratio, 5);
}

static inline float quartic_io(float step, float max_steps, float max_val)
{
	float ratio = step / (max_steps / 2.0);

	if (ratio < 1)
		return max_val / 2 * pow(ratio, 4);

	return max_val + max_val / 2 * pow(ratio -2, 5);
}

/* Quintic */
static inline float quintic_in(float step, float max_steps, float max_val)
{
	float ratio = step / max_steps;

	return max_val * pow(ratio, 5);
}

static inline float quintic_out(float step, float max_steps, float max_val)
{
	float ratio = (step / max_steps) - 1.0;

	return max_val + max_val * pow(ratio, 5);
}

static inline float quintic_io(float step, float max_steps, float max_val)
{
	float ratio = step / (max_steps / 2.0);

	if (ratio < 1)
		return max_val / 2 * pow(ratio, 5);

    ratio -= 2;
	return max_val + max_val / 2 * pow(ratio, 5);
}

/* Circular */
static inline float circular_in(float step, float max_steps, float max_val)
{
	float ratio = step / max_steps;

	return - max_val * (sqrt(1 - pow(ratio, 2) ) - 1);
}

static inline float circular_out(float step, float max_steps, float max_val)
{
	float ratio = (step - max_steps) / (max_steps - 1.0);
	return max_val * sqrt(1 - pow(ratio, 2));
}

static inline float circular_io(float step, float max_steps, float max_val)
{
	float ratio = step / (max_steps / 2.0);

	if (ratio < 1)
		return - max_val / 2 * (sqrt(1 - pow(ratio, 2)) - 1);

	return max_val / 2 * (sqrt(1 - pow(ratio - 2, 2)) + 1);
}

/* Sine */
static inline float sine_in(float step, float max_steps, float max_val)
{
	return -max_val * cos((float)step / max_steps * M_PI_2) + max_val;
}

static inline float sine_out(float step, float max_steps, float max_val)
{
	return max_val * sin((((float)step / max_steps)) * M_PI_2);
}

static inline float sine_io(float step, float max_steps, float max_val)
{
	return -max_val / 2 * (cos(M_PI * step / max_steps) - 1);
}

/* Bounce */
static inline float bounce_out(float step, float max_steps, float max_val)
{
	float ratio = step / max_steps;

	if (ratio < (1 / 2.75)) {
		return max_val * (7.5625 * ratio * ratio);
	}

    if (ratio < (2 / 2.75)) {
        ratio -= 1.5 / 2.75;
		return max_val * (7.5625 * ratio * ratio + .75);
    }

    if (ratio < (2.5 / 2.75)) {
        ratio -= 2.25 / 2.75;
		return max_val * (7.5625 * ratio * ratio + .9375);
	}

    ratio -= 2.625 / 2.75;
    return max_val * (7.5625 * ratio * ratio + .984375);
}

static inline float bounce_in(float step, float max_steps, float max_val)
{
	return max_val - bounce_out(max_steps - step, max_steps, max_val);
}

static inline float bounce_io(float step, float max_steps, float max_val)
{
	if (step < max_steps / 2)
		return bounce_in(step * 2, max_steps, max_val) * 0.5;
	else
		return bounce_out(step * 2 - max_steps, max_steps, max_val) *
            0.5 + max_val * 0.5;
}

/* Back */
static inline float back_in(float step, float max_steps, float max_val)
{
	float s = 1.70158f;
    float ratio = step / max_steps;
	return max_val * ratio * ratio * ((s + 1) * ratio - s);

}

static inline float back_out(float step, float max_steps, float max_val)
{
	float s = 1.70158f;
	float ratio = (step / max_steps) - 1;
	return max_val * (ratio * ratio * ((s + 1) * ratio + s) + 1);
}

static inline float back_io(float step, float max_steps, float max_val)
{
    float s = 1.70158f * 1.525;
    float ratio = step / (max_steps / 2);

	if (ratio < 1)
        return max_val / 2 * (ratio * ratio * ((s + 1) * ratio - s));

	ratio -= 2;
	return max_val / 2 * (ratio * ratio * ((s + 1) * ratio + s) + 2);
}

/* Float Functions */

/* Custom, used for breathing */
float
exponential_custom_f_io(float step, float max_steps, float max_val)
{
    return exponential_custom_io(step, max_steps, max_val);
}
float
exp_sin_custom_f_io(float step, float max_steps, float max_val)
{
    return exp_sin_custom_io(step, max_steps, max_val);
}
float
sine_custom_f_io(float step, float max_steps, float max_val)
{
    return sine_custom_io(step, max_steps, max_val);
}

/* Linear */
float
linear_f_io(float step, float max_steps, float max_val)
{
    return linear_io(step, max_steps, max_val);
}

/* Exponential */
float
exponential_f_in(float step, float max_steps, float max_val)
{
    return exponential_in(step, max_steps, max_val);
}
float
exponential_f_out(float step, float max_steps, float max_val)
{
    return exponential_out(step, max_steps, max_val);
}
float
exponential_f_io(float step, float max_steps, float max_val)
{
    return exponential_io(step, max_steps, max_val);
}

/* Quadratic */
float
quadratic_f_in(float step, float max_steps, float max_val)
{
    return quadratic_in(step, max_steps, max_val);
}
float
quadratic_f_out(float step, float max_steps, float max_val)
{
    return quadratic_out(step, max_steps, max_val);
}
float
quadratic_f_io(float step, float max_steps, float max_val)
{
    return quadratic_io(step, max_steps, max_val);
}

/* Cubic */
float
cubic_f_in(float step, float max_steps, float max_val)
{
    return cubic_in(step, max_steps, max_val);
}
float
cubic_f_out(float step, float max_steps, float max_val)
{
    return cubic_out(step, max_steps, max_val);
}
float
cubic_f_int_io(float step, float max_steps, float max_val)
{
    return cubic_io(step, max_steps, max_val);
}

/* Quartic */
float
quartic_f_in(float step, float max_steps, float max_val)
{
    return quartic_in(step, max_steps, max_val);
}
float
quartic_f_out(float step, float max_steps, float max_val)
{
    return quartic_out(step,
                       max_steps,
                       max_val);
}
float
quartic_f_io(float step, float max_steps, float max_val)
{
    return quartic_io(step, max_steps, max_val);
}

/* Quintic */
float
quintic_f_in(float step, float max_steps, float max_val)
{
    return quintic_in(step, max_steps, max_val);
}
float
quintic_f_out(float step, float max_steps, float max_val)
{
    return quintic_out(step, max_steps, max_val);
}
float
quintic_f_io(float step, float max_steps, float max_val)
{
    return quintic_io(step, max_steps, max_val);
}

/* Circular */
float
circular_f_in(float step, float max_steps, float max_val)
{
    return circular_in(step, max_steps, max_val);
}
float
circular_f_out(float step, float max_steps, float max_val)
{
    return circular_out(step, max_steps, max_val);
}
float
circular_f_io(float step, float max_steps, float max_val)
{
    return circular_io(step, max_steps, max_val);
}

/* Sine */
float
sine_f_in(float step, float max_steps, float max_val)
{
    return sine_in(step, max_steps, max_val);
}
float
sine_f_out(float step, float max_steps, float max_val)
{
    return sine_out(step, max_steps, max_val);
}
float
sine_f_io(float step, float max_steps, float max_val)
{
    return sine_io(step, max_steps, max_val);
}

/* Bounce */
float
bounce_f_in(float step, float max_steps, float max_val)
{
    return bounce_in(step, max_steps, max_val);
}
float
bounce_f_out(float step, float max_steps, float max_val)
{
    return bounce_out(step, max_steps, max_val);
}
float
bounce_f_io(float step, float max_steps, float max_val)
{
    return bounce_io(step, max_steps, max_val);
}

/* Back */
float
back_f_in(float step, float max_steps, float max_val)
{
    return back_in(step, max_steps, max_val);
}
float
back_f_out(float step, float max_steps, float max_val)
{
    return back_out(step, max_steps, max_val);
}
float
back_f_io(float step, float max_steps, float max_val)
{
    return back_io(step, max_steps, max_val);
}

/* Integer Functions */
/* Custom */
int32_t
exponential_custom_int_io(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) exponential_custom_io((float) step,
                                           (float) max_steps,
                                           (float) max_val);
}
int32_t
exp_sin_custom_int_io(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) exp_sin_custom_io((float) step,
                                       (float) max_steps,
                                       (float) max_val);
}
int32_t
sine_custom_int_io(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) sine_custom_io((float) step,
                                    (float) max_steps,
                                    (float) max_val);
}

/* Linear */
int32_t
linear_int_io(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) linear_io((float) step,
                               (float) max_steps,
                               (float) max_val);
}

/* Exponential */
int32_t
exponential_int_in(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) exponential_in((float) step,
                                    (float) max_steps,
                                    (float) max_val);
}
int32_t
exponential_int_out(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) exponential_out((float) step,
                                     (float) max_steps,
                                     (float) max_val);
}
int32_t
exponential_int_io(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) exponential_io((float) step,
                                    (float) max_steps,
                                    (float) max_val);
}

/* Quadratic */
int32_t
quadratic_int_in(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) quadratic_in((float) step,
                                  (float) max_steps,
                                  (float) max_val);
}
int32_t
quadratic_int_out(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) quadratic_out((float) step,
                                   (float) max_steps,
                                   (float) max_val);
}
int32_t
quadratic_int_io(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) quadratic_io((float) step,
                                  (float) max_steps,
                                  (float) max_val);
}

/* Cubic */
int32_t
cubic_int_in(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) cubic_in((float) step,
                              (float) max_steps,
                              (float) max_val);
}
int32_t
cubic_int_out(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) cubic_out((float) step,
                               (float) max_steps,
                               (float) max_val);
}
int32_t
cubic_int_io(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) cubic_io((float) step,
                              (float) max_steps,
                              (float) max_val);
}

/* Quartic */
int32_t
quartic_int_in(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) quartic_in((float) step,
                                (float) max_steps,
                                (float) max_val);
}
int32_t
quartic_int_out(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) quartic_out((float) step,
                                 (float) max_steps,
                                 (float) max_val);
}
int32_t
quartic_int_io(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) quartic_io((float) step,
                                (float) max_steps,
                                (float) max_val);
}

/* Quintic */
int32_t
quintic_int_in(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) quintic_in((float) step,
                                (float) max_steps,
                                (float) max_val);
}
int32_t
quintic_int_out(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) quintic_out((float) step,
                                 (float) max_steps,
                                 (float) max_val);
}
int32_t
quintic_int_io(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) quintic_io((float) step,
                                (float) max_steps,
                                (float) max_val);
}

/* Circular */
int32_t
circular_int_in(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) circular_in((float) step,
                                 (float) max_steps,
                                 (float) max_val);
}
int32_t
circular_int_out(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) circular_out((float) step,
                                  (float) max_steps,
                                  (float) max_val);
}
int32_t
circular_int_io(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) circular_io((float) step,
                                 (float) max_steps,
                                 (float) max_val);
}

/* Sine */
int32_t
sine_int_in(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) sine_in((float) step,
                             (float) max_steps,
                             (float) max_val);
}
int32_t
sine_int_out(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) sine_out((float) step,
                              (float) max_steps,
                              (float) max_val);
}
int32_t
sine_int_io(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) sine_io((float) step,
                             (float) max_steps,
                             (float) max_val);
}

/* Bounce */
int32_t
bounce_int_in(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) bounce_in((float) step,
                               (float) max_steps,
                               (float) max_val);
}
int32_t
bounce_int_out(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) bounce_out((float) step,
                                (float) max_steps,
                                (float) max_val);
}
int32_t
bounce_int_io(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) bounce_io((float) step,
                               (float) max_steps,
                               (float) max_val);
}

/* Back */
int32_t
back_int_in(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) back_in((float) step,
                             (float) max_steps,
                             (float) max_val);
}
int32_t
back_int_out(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) back_out((float) step,
                              (float) max_steps,
                              (float) max_val);
}
int32_t
back_int_io(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (int32_t) back_io((float) step,
                             (float) max_steps,
                             (float) max_val);
}
