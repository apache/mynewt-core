#ifndef H_OS_MALLOC_
#define H_OS_MALLOC_

#include "os/os_heap.h"

#undef  malloc
#define malloc  os_malloc

#undef  free
#define free    os_free

#undef  realloc
#define realloc  os_realloc

#endif
