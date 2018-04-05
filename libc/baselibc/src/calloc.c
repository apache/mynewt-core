/*
 * calloc.c
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

void *calloc(size_t nmemb, size_t size)
{
	void *ptr;
        int nb;

        nb = sizeof(size_t) * 4;
        if (size >= SIZE_MAX >> nb || nmemb >= SIZE_MAX >> nb) {
            return NULL;
        }
	size *= nmemb;
	ptr = malloc(size);
	if (ptr)
		memset(ptr, 0, size);

	return ptr;
}
