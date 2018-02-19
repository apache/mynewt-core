#include "easing/easing.h"
#include <math.h>

/* Custom */
int32_t exponential_custom_io(int32_t step, int32_t max_steps, int32_t max_val)
{
    float R = (float) max_steps * log10(2) / log10((float) max_val);
    return pow(2, ((float) step / R)) - 1;
}

int32_t exp_sin_custom_io(int32_t step, int32_t max_steps, int32_t max_val)
{
    float mplier = max_val / 2.35040238729f;
    float pi_d_maxs = M_PI / max_steps;
    step += max_steps;
    return ( exp( sin((step * pi_d_maxs) + M_PI_2)) - ONE_DIV_E ) * mplier;
}

int32_t sine_custom_io(int32_t step, int32_t max_steps, int32_t max_val)
{
    return max_val * cos((PI_TIMES2 * step/max_steps) + M_PI) + max_val;
}

/* Linear */
int32_t linear_io(int32_t step, int32_t max_steps, int32_t max_val)
{
	return step * max_val / max_steps;
}

/* Exponential */
int32_t exponential_in(int32_t step, int32_t max_steps, int32_t max_val)
{
    return (step == 0) ?
        0 :
        pow(max_val, (float)step/max_steps);
}

int32_t exponential_out(int32_t step, int32_t max_steps, int32_t max_val)
{
	return (step == max_steps) ?
        max_val :
        max_val - pow(max_val, 1.0 - (float)step/max_steps);
}

int32_t exponential_io(int32_t step, int32_t max_steps, int32_t max_val)
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
int32_t quadratic_in(int32_t step, int32_t max_steps, int32_t max_val)
{
	float ratio = (float) step / max_steps;

	return max_val * pow(ratio, 2);
}

int32_t quadratic_out(int32_t step, int32_t max_steps, int32_t max_val)
{
    float ratio = (float) step / max_steps;

	return -max_val * ratio * (ratio - 2.0);
}

int32_t quadratic_io(int32_t step, int32_t max_steps, int32_t max_val)
{
	float ratio = (float) step / (max_steps / 2.0);

	if (ratio < 1)
		return max_val / 2.0 * pow(ratio, 2);

    ratio = (step - (max_steps/2.0)) / (max_steps/2.0);
    return (max_val / 2.0) - (max_val / 2.0) * ratio * (ratio - 2.0);
}

/* Cubic */
int32_t cubic_in(int32_t step, int32_t max_steps, int32_t max_val)
{
	float ratio = (float) step / max_steps;

	return max_val * pow(ratio, 3);
}

int32_t cubic_out(int32_t step, int32_t max_steps, int32_t max_val)
{
	float ratio = (float) step / (max_steps - 1.0);

	return max_val * (pow(ratio, 3) + 1);
}

int32_t cubic_io(int32_t step, int32_t max_steps, int32_t max_val)
{
	float ratio = (float) step / (max_steps / 2.0);

	if (ratio < 1)
		return max_val / 2 * pow(ratio, 3);

	return max_val / 2 * (pow(ratio - 2, 3) + 2);
}

/* Quartic */
int32_t quartic_in(int32_t step, int32_t max_steps, int32_t max_val)
{
	float ratio = (float) step / max_steps;

	return max_val * pow(ratio, 4);
}

int32_t quartic_out(int32_t step, int32_t max_steps, int32_t max_val)
{
	float ratio = ((float) step / max_steps) - 1.0;

	return max_val + max_val * pow(ratio, 5);
}

int32_t quartic_io(int32_t step, int32_t max_steps, int32_t max_val)
{
	float ratio = (float) step / (max_steps / 2.0);

	if (ratio < 1)
		return max_val / 2 * pow(ratio, 4);

	return max_val + max_val / 2 * pow(ratio -2, 5);
}

/* Quintic */
int32_t quintic_in(int32_t step, int32_t max_steps, int32_t max_val)
{
	float ratio = (float) step / max_steps;

	return max_val * pow(ratio, 5);
}

int32_t quintic_out(int32_t step, int32_t max_steps, int32_t max_val)
{
	float ratio = ((float) step / max_steps) - 1.0;

	return max_val + max_val * pow(ratio, 5);
}

int32_t quintic_io(int32_t step, int32_t max_steps, int32_t max_val)
{
	float ratio = (float) step / (max_steps / 2.0);

	if (ratio < 1)
		return max_val / 2 * pow(ratio, 5);

    ratio -= 2;
	return max_val + max_val / 2 * pow(ratio, 5);
}

/* Circular */
int32_t circular_in(int32_t step, int32_t max_steps, int32_t max_val)
{
	float ratio = (float) step / max_steps;

	return - max_val * (sqrt(1 - pow(ratio, 2) ) - 1);
}

int32_t circular_out(int32_t step, int32_t max_steps, int32_t max_val)
{
	float ratio = ((float) step - max_steps) / (max_steps - 1.0);
	return max_val * sqrt(1 - pow(ratio, 2));
}

int32_t circular_io(int32_t step, int32_t max_steps, int32_t max_val)
{
	float ratio = (float) step / (max_steps / 2.0);

	if (ratio < 1)
		return - max_val / 2 * (sqrt(1 - pow(ratio, 2)) - 1);

	return max_val / 2 * (sqrt(1 - pow(ratio - 2, 2)) + 1);
}

/* Sine */
int32_t sine_in(int32_t step, int32_t max_steps, int32_t max_val)
{
	return -max_val * cos((float)step / max_steps * M_PI_2) + max_val;
}

int32_t sine_out(int32_t step, int32_t max_steps, int32_t max_val)
{
	return max_val * sin((((float)step / max_steps)) * M_PI_2);
}

int32_t sine_io(int32_t step, int32_t max_steps, int32_t max_val)
{
	return -max_val / 2 * (cos(M_PI * step / max_steps) - 1);
}


/* Bounce */
int32_t bounce_in(int32_t step, int32_t max_steps, int32_t max_val)
{
	return max_val - bounce_out(max_steps - step, max_steps, max_val);
}

int32_t bounce_out(int32_t step, int32_t max_steps, int32_t max_val)
{

	float ratio = (float) step / max_steps;

	if (ratio < (1 / 2.75)) {
		return max_val * (7.5625 * ratio * ratio);
	}

    if (ratio < (2 / 2.75)) {
        ratio -= 1.5 / 2.75;
		return (float) max_val * (7.5625 * ratio * ratio + .75);
    }

    if (ratio < (2.5 / 2.75)) {
        ratio -= 2.25 / 2.75;
		return (float) max_val * (7.5625 * ratio * ratio + .9375);
	}

    ratio -= 2.625 / 2.75;
    return (float) max_val * (7.5625 * ratio * ratio + .984375);
}

int32_t bounce_io(int32_t step, int32_t max_steps, int32_t max_val)
{
	if (step < max_steps / 2)
		return bounce_in(step * 2, max_steps, max_val) * 0.5;
	else
		return bounce_out(step * 2 - max_steps, max_steps, max_val) *
            0.5 + max_val * 0.5;
}

/* Back */
int32_t back_in(int32_t step, int32_t max_steps, int32_t max_val)
{
	float s = 1.70158f;
    float ratio = (float) step / max_steps;
	return (float) max_val * ratio * ratio * ((s + 1) * ratio - s);

}

int32_t back_out(int32_t step, int32_t max_steps, int32_t max_val)
{
	float s = 1.70158f;
	float ratio = ((float) step / max_steps) - 1;
	return (float) max_val * (ratio * ratio * ((s + 1) * ratio + s) + 1);
}

int32_t back_io(int32_t step, int32_t max_steps, int32_t max_val)
{
    float s = 1.70158f * 1.525;
    float ratio = (float) step / (max_steps / 2);

	if (ratio < 1)
        return (float) max_val / 2 * (ratio * ratio * ((s + 1) * ratio - s));

	ratio -= 2;
	return (float) max_val / 2 * (ratio * ratio * ((s + 1) * ratio + s) + 2);
}
