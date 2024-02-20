/*
 * sprintf.c
 */

#include <stdio.h>
#include <stdint.h>
#include <limits.h>

int sprintf(char *buffer, const char *format, ...)
{
	va_list ap;
	int rv;

	va_start(ap, format);
	rv = vsnprintf(buffer, INT_MAX/2, format, ap);
	va_end(ap);

	return rv;
}
