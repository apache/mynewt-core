/*
 * vsprintf.c
 */

#include <stdio.h>
#include <stdint.h>
#include <limits.h>

int vsprintf(char *buffer, const char *format, va_list ap)
{
	return vsnprintf(buffer, INT_MAX/2, format, ap);
}
