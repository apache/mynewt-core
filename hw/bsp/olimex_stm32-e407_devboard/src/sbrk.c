#include <errno.h>

extern char __HeapBase;
extern char __HeapLimit;

void *
_sbrk(int incr)
{
    static char *brk = &__HeapBase;

    void *prev_brk;

    if (incr < 0) {
        /* Returning memory to the heap. */
        incr = -incr;
        if (brk - incr < &__HeapBase) {
            prev_brk = (void *)-1;
            errno = EINVAL;
        } else {
            prev_brk = brk;
            brk -= incr;
        }
    } else {
        /* Allocating memory from the heap. */
        if (&__HeapLimit - brk >= incr) {
            prev_brk = brk;
            brk += incr;
        } else {
            prev_brk = (void *)-1;
            errno = ENOMEM;
        }
    }

    return prev_brk;
}
