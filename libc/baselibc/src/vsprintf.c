/*
 * vsprintf.c
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

int vsprintf(char *buffer, const char *format, va_list ap)
{
	return vsnprintf(buffer, SIZE_MAX/2, format, ap);
}
