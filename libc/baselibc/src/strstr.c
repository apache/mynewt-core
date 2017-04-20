/*
 * strstr.c
 */

#include <string.h>

char *strstr(const char *haystack, const char *needle)
{
	return (char *)memmem(haystack, strlen(haystack), needle,
			      strlen(needle));
}

char *strnstr(const char *haystack, const char *needle, size_t len)
{
	return (char *)memmem(haystack, strlen(haystack), needle, len);
}
