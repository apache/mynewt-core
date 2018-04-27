#ifndef _GNSS_Q_H_
#define _GNSS_Q_H_

#include <stdint.h>

/*
 * https://en.wikipedia.org/wiki/Q_(number_format)
 */

#define GNSS_Qm    15
#define GNSS_Qn    17

#define GNSS_L_TO_Q(m) ((m) << GNSS_Qm)

/**
 * Minimum value for Q number
 */
#define GNSS_Q_MIN INT32_MIN

/**
 * Maximam value for Q number
 */
#define GNSS_Q_MAX INT32_MAX

/**
 * Q number type
 */
typedef int32_t gnss_q_t;

/**
 * Adding Q number.
 */
gnss_q_t gnss_q_add(gnss_q_t a, gnss_q_t b);

/**
 * Substracting Q number.
 */
gnss_q_t gnss_q_sub(gnss_q_t a, gnss_q_t b);

/**
 * Multiplying Q number.
 */
gnss_q_t gnss_q_mul(gnss_q_t a, gnss_q_t b);

/**
 * Dividing Q number.
 */
gnss_q_t gnss_q_div(gnss_q_t a, gnss_q_t b);

/**
 * Converting a float to a Q number
 */
gnss_q_t gnss_f_to_q(float f);

/**
 * Converting a long to a Q number.
 */
gnss_q_t gnss_l_to_q(long  q);

/**
 * Converting a Q number to a long (fractional part is truncated).
 */
static inline long
gnss_q_to_l(gnss_q_t q)
{
    return q >> GNSS_Qm;
}

/**
 * Converting a Q number to a float.
 */
static inline float
gnss_q_to_f(gnss_q_t q)
{
    return ((float)q) / (1 << GNSS_Qm);
}

/**
 * Adding Q number (possible overflow wrapping).
 */
static inline gnss_q_t
gnss_q_add_nosat(gnss_q_t a, gnss_q_t b)
{
    return a + b;
}

/**
 * Substracting Q number (possible overflow wrapping).
 */
static inline gnss_q_t
gnss_q_sub_nosat(gnss_q_t a, gnss_q_t b)
{
    return a - b;
}

/**
 * Parse a string to extract the Q number
 *
 * @param str			String to parse
 * @param endptr		Last character parsed (if not NULL)
 *
 * @return Q number
 */
gnss_q_t gnss_q_parse(const char *str, char **endptr);

#endif
