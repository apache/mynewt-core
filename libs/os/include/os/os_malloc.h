#ifndef H_OS_MALLOC_
#define H_OS_MALLOC_

#include <stddef.h>

void *os_malloc(size_t size);
void os_free(void *mem);
void *os_realloc(void *ptr, size_t size);

#endif

