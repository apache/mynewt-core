#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "gnss/q.h"

/* Maximum number of digits */
// XXX is it a compile-time constante?
#define GNSS_Q_FDIGITS ((int)((GNSS_Qm / 3.32192809489) + 1))

static inline gnss_q_t
gnss_q_sat(int64_t x)
{
    if        (x > GNSS_Q_MAX) {
        return GNSS_Q_MAX;
    } else if (x < GNSS_Q_MIN) {
        return GNSS_Q_MIN;
    } else {
        return (gnss_q_t)x;
    }
}

gnss_q_t
gnss_q_add(gnss_q_t a, gnss_q_t b)
{
    return gnss_q_sat((int64_t)a + (int64_t)b);
}

gnss_q_t
gnss_q_sub(gnss_q_t a, gnss_q_t b)
{
    return gnss_q_sat((int64_t)a - (int64_t)b);
}

gnss_q_t
gnss_q_mul(gnss_q_t a, gnss_q_t b)
{
    int64_t tmp;
    tmp = (int64_t)a * (int64_t)b;    
    tmp += 1 << (GNSS_Qm - 1);
    return gnss_q_sat(tmp >> GNSS_Qm);
}

gnss_q_t
gnss_q_div(gnss_q_t a, gnss_q_t b)
{
    int64_t tmp;
    tmp = (int64_t)a << GNSS_Qm;
    if((tmp >= 0 && b >= 0) || (tmp < 0 && b < 0)) {   
        tmp += b / 2;
    } else {
        tmp -= b / 2;
    }
    return (gnss_q_t)(tmp / b);
}

gnss_q_t
gnss_f_to_q(float f)
{
    return gnss_q_sat(roundf(f * (1 << GNSS_Qm)));
}

gnss_q_t
gnss_l_to_q(long  q)
{
    return gnss_q_sat((int64_t)q << GNSS_Qm);
}

gnss_q_t
gnss_q_parse(const char *str, char **endptr)
{
    char *eob;
    int64_t v;

    /* Integer part */
    v = (int64_t)strtol(str, &eob, 10);
    
    /* What's next? */
    switch(*eob) {
    case '\0':
	v *= (1 << GNSS_Qm);
	str = eob;
	goto done;
    case '.':
	str = eob + 1;
	break;
    default:
	goto fail;
    }

    /* Fractional part */
    /*  limit to the meaningful digits */
    eob = (char *)(str + GNSS_Q_FDIGITS);
    while ((str < eob) && isdigit(*str)) {
	v = (v * 10) + (*str++ - '0');
    }
    v *= (1 << GNSS_Qm);
    while (eob++ - str < GNSS_Q_FDIGITS) {
	v  /= 10;
    }
    /*  consume remaining digits */
    while (isdigit(*str)) {
	str++;
    }
    if (*str != '\0') {
    fail:
	if (endptr) {
	    *endptr = (char *)str;
	}
	return 0;
    }
    
 done:
    if (endptr) {
	*endptr = (char *)str;
    }
    return gnss_q_sat(v);
}
