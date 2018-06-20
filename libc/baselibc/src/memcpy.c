/*
 * memcpy.c
 */

#include <string.h>
#include <stdint.h>

void *memcpy(void *dst, const void *src, size_t n)
{
	const char *p = src;
	char *q = dst;
#if defined(__i386__)
	size_t nl = n >> 2;
	asm volatile ("cld ; rep ; movsl ; movl %3,%0 ; rep ; movsb":"+c" (nl),
		      "+S"(p), "+D"(q)
		      :"r"(n & 3));
#elif defined(__x86_64__)
	size_t nq = n >> 3;
	asm volatile ("cld ; rep ; movsq ; movl %3,%%ecx ; rep ; movsb":"+c"
		      (nq), "+S"(p), "+D"(q)
		      :"r"((uint32_t) (n & 7)));
#elif defined(ARCH_cortex_m0) || defined(ARCH_cortex_m3) || defined(ARCH_cortex_m4) || defined(ARCH_cortex_m7)
        (void)p;
        (void)q;

#if defined(ARCH_cortex_m3) || defined(ARCH_cortex_m4) || defined(ARCH_cortex_m7)
        /*
         * For Cortex-M3/4/7 we can speed up a bit by moving 32-bit words since
         * it supports unaligned access.
         */
        asm (".syntax unified           \n"
             "       b    test1         \n"
             "loop1: ldr  r3, [r1, r2]  \n"
             "       str  r3, [r0, r2]  \n"
             "test1: subs r2, #4        \n"
             "       bpl  loop1         \n"
             "       add  r2, #4        \n"
            );
#endif

        asm (".syntax unified           \n"
             "       b    test2         \n"
             "loop2: ldrb r3, [r1, r2]  \n"
             "       strb r3, [r0, r2]  \n"
             "test2: subs r2, #1        \n"
             "       bpl  loop2         \n"
            );
#else
	while (n--) {
		*q++ = *p++;
	}
#endif

	return dst;
}
