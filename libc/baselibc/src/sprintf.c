/*
 * sprintf.c
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

int sprintf(char *buffer, const char *format, ...)
{
	va_list ap;
	int rv;

	va_start(ap, format);
	rv = vsnprintf(buffer, SIZE_MAX/2, format, ap);
	va_end(ap);

	return rv;
}
