/*
 * strtok.c
 */

#include <string.h>

char *strtok(char *s, const char *delim)
{
	static char *holder;

	return strtok_r(s, delim, &holder);
}
