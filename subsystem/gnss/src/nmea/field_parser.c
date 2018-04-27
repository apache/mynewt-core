#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <syscfg/syscfg.h>
#include <gnss/nmea.h>

bool
gnss_nmea_field_parse_string(const char *str, char *val, uint16_t maxsize)
{
    assert(val != NULL);
    assert(maxsize > 0);
    
    for ( ; (maxsize > 0) && (*str != '\0') ; maxsize--) {
	*val++ = *str++;
    }
    if (maxsize > 0) {
	*val = '\0';
    }
    return *str == '\0';
}

bool
gnss_nmea_field_parse_char(const char *str, char *val)
{
    char v = *str;
    if (val) { *val = v; }
    return str[v ? 1 : 0] == '\0';
}

bool
gnss_nmea_field_parse_long(const char *str, long *val)
{
    char *eob;
    long v = strtol(str, &eob, 10);
    if (val) { *val = v; }
    return *eob == '\0';
}

bool
gnss_nmea_field_parse_crc(const char *str, uint8_t *val)
{
    char *eob;
    long v = strtoul(str, &eob, 16);
    if (v > 255) return false;
    if (val) { *val = v; }
    return *eob == '\0';
}

bool
gnss_nmea_field_parse_float(const char *str, gnss_float_t *val)
{
    char *eob;
#if MYNEWT_VAL(GNSS_USE_FLOAT) > 0
    float v = strtod(str, &eob);
    if (val) { *val = v; }
    return *eob == '\0';
#else
    gnss_q_t v = gnss_q_parse(str, &eob);
    if (val) { *val = v; }
    return *eob == '\0';
#endif
}

bool
gnss_nmea_field_parse_and_apply_direction(const char *str, gnss_float_t *val)
{
    int8_t v = 0;
    switch (str[0]) {
    case 'N': case 'E': v =  1; break;
    case 'S': case 'W': v = -1; break;
    case '\0': break;
    default: return false;
    }
    if (val) { *val = *val * v; }
    return str[v ? 1 : 0] == '\0';
}

bool
gnss_nmea_field_parse_latlng(const char *str, gnss_float_t *val)
{
#if MYNEWT_VAL(GNSS_USE_FLOAT) > 0
    char *eob;
    float v = strtod(str, &eob);
    if (val) {
	int dd  = v / 100;
	*val = dd + (v - dd * 100) / 60.0f;
    }
    return *eob == '\0';    
#else
    gnss_q_t v;
    if (!gnss_nmea_field_parse_float(str, &v)) {
	return false;
    }
    if (val) {
	int32_t dd = gnss_q_to_l(v) / 100;
	v = gnss_q_sub_nosat(v, gnss_l_to_q(dd * 100));
	v = gnss_q_div(v, GNSS_L_TO_Q(60));
	*val = gnss_q_add_nosat(gnss_l_to_q(dd), v);
    }
    return true;
#endif
}

bool
gnss_nmea_field_parse_date(const char *str, gnss_date_t *val)
{
    char *eob;
    gnss_date_t v = { .present = *str != '\0' };
    unsigned long n = strtoul(str, &eob, 10);
    if (val) { v.year    = (n % 100) + MYNEWT_VAL(GNSS_NMEA_YEAR_OFFSET);
	       v.month   = (n / 100) % 100;
	       v.day     =  n / 10000;
	       *val = v; }
    return *eob == '\0';
}

bool
gnss_nmea_field_parse_time(const char *str, gnss_time_t *val)
{
    gnss_time_t v = { .present = *str != '\0' };
    char *eob;
    unsigned long a,b = 0;

    a = strtoul(str, &eob, 10);
    if (a > 240000) {
	return false;
    }
    switch(*eob) {
    case '\0': goto done;
    case '.' : str = eob + 1; break;
    default  : return false;
    }
    eob = (char *)(str + 6);
    while ((str < eob) && isdigit(*str)) {
	b = (b * 10) + (*str++ - '0');
    }
    while (isdigit(*str)) {
	str++;
    }
    if (*str != '\0')
	return false;

 done:
    if (val) {	
	v.seconds      =  a % 100;
	a /= 100;
	v.minutes      =  a % 100;
	v.hours        =  a / 100;
	v.microseconds = b;
	*val = v;
    }
    return true;
}
