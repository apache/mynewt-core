#include <assert.h>
#include "os/os_mutex.h"
#include "os/os_heap.h"

extern struct os_task *g_current_task;  /* XXX */

static struct os_mutex os_malloc_mutex;

static void
os_malloc_lock(void)
{
    int rc;

    if (g_current_task != NULL) { /* XXX */
        rc = os_mutex_pend(&os_malloc_mutex, 0xffffffff);
        assert(rc == 0);
    }
}

static void
os_malloc_unlock(void)
{
    int rc;

    if (g_current_task != NULL) { /* XXX */
        rc = os_mutex_release(&os_malloc_mutex);
        assert(rc == 0);
    }
}

void *
os_malloc(size_t size)
{
    void *ptr;

    os_malloc_lock();
    ptr = malloc(size);
    os_malloc_unlock();

    return ptr;
}

void
os_free(void *mem)
{
    os_malloc_lock();
    free(mem);
    os_malloc_unlock();
}

void *
os_realloc(void *ptr, size_t size)
{
    void *new_ptr;

    os_malloc_lock();
    new_ptr = realloc(ptr, size);
    os_malloc_unlock();

    return new_ptr;
}
