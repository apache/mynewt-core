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
#elif defined(__arm__)
        (void)p;
        (void)q;

        asm (".syntax unified           \n"
             "       push {r0, r4}      \n"
#if !defined(__ARM_FEATURE_UNALIGNED)
             /*
              * For Cortex-M0 check if copy by 32-bit is possible, if not
              * copy everything by byte.
              */
             "       movs r3, r0        \n"
             "       orrs r3, r1        \n"
             "       lsls r3, #30       \n"
             "       bne  2f            \n"
#endif
             "       lsrs r3, r2, #2    \n"
             "       beq  2f            \n"
             "       lsls r4, r3, #2    \n"
             "       subs r2, r2, r4    \n"
             "       rsbs r4, #0        \n"
             "       subs r0, r4        \n"
             "       subs r1, r4        \n"
             "1:     ldr  r3, [r1, r4]  \n"
             "       str  r3, [r0, r4]  \n"
             "       adds r4, #4        \n"
             "       bmi  1b            \n"
             "2:     rsbs r2, #0        \n"
             "       beq  4f            \n"
             "       subs r0, r2        \n"
             "       subs r1, r2        \n"
             "3:     ldrb r3, [r1, r2]  \n"
             "       strb r3, [r0, r2]  \n"
             "       adds r2, #1        \n"
             "       bmi  3b            \n"
             "4:     pop  {r0, r4}      \n"
            );
#else
	while (n--) {
		*q++ = *p++;
	}
#endif

	return dst;
}
