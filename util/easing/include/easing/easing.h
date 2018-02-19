#include <stdint.h>

#ifndef _UTIL_EASING_INT_H_
#define _UTIL_EASING_INT_H_

typedef int32_t (*easing_func_t)(int32_t step, int32_t max_steps, int32_t max_val);

/* Custom, used for breathing */
int32_t exponential_custom_io(int32_t step, int32_t max_steps, int32_t max_val);
int32_t exp_sin_custom_io(int32_t step, int32_t max_steps, int32_t max_val);
int32_t sine_custom_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Linear */
int32_t linear_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Exponential */
int32_t exponential_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t exponential_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t exponential_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Quadratic */
int32_t quadratic_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t quadratic_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t quadratic_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Cubic */
int32_t cubic_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t cubic_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t cubic_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Quartic */
int32_t quartic_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t quartic_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t quartic_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Quintic */
int32_t quintic_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t quintic_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t quintic_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Circular */
int32_t circular_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t circular_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t circular_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Sine */
int32_t sine_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t sine_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t sine_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Bounce */
int32_t bounce_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t bounce_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t bounce_io(int32_t step, int32_t max_steps, int32_t max_val);

/* Back */
int32_t back_in(int32_t step, int32_t max_steps, int32_t max_val);
int32_t back_out(int32_t step, int32_t max_steps, int32_t max_val);
int32_t back_io(int32_t step, int32_t max_steps, int32_t max_val);

int32_t back_out_custom(int32_t step, int32_t max_steps, int32_t max_val);
#endif /* _UTIL_EASING_INT_H_ */
