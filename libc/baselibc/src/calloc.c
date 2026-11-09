/*
 * calloc.c
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

void *calloc(size_t nmemb, size_t size)
{
	void *ptr;

	if (nmemb && size > SIZE_MAX / nmemb) {
		return NULL;
	}
	size *= nmemb;
	ptr = malloc(size);
	if (ptr)
		memset(ptr, 0, size);

	return ptr;
}
