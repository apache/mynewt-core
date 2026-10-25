/*
 * strndup.c
 */

#include <string.h>
#include <stdlib.h>

char *strndup(const char *s, size_t n)
{
	size_t l = strnlen(s, n);
	char *d = malloc(l + 1);

	if (!d)
		return NULL;

	memcpy(d, s, l);
	d[l] = '\0';
	return d;
}
