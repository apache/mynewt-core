/*
 * memset.c
 */

#include <string.h>
#include <stdint.h>

#if defined(__arm__)
#include <mcu/cmsis_nvic.h>
#endif

void *memset(void *dst, int c, size_t n)
{
	char *q = dst;

#if defined(__i386__)
	size_t nl = n >> 2;
	asm volatile ("cld ; rep ; stosl ; movl %3,%0 ; rep ; stosb"
		      : "+c" (nl), "+D" (q)
		      : "a" ((unsigned char)c * 0x01010101U), "r" (n & 3));
#elif defined(__x86_64__)
	size_t nq = n >> 3;
	asm volatile ("cld ; rep ; stosq ; movl %3,%%ecx ; rep ; stosb"
		      :"+c" (nq), "+D" (q)
		      : "a" ((unsigned char)c * 0x0101010101010101U),
			"r" ((uint32_t) n & 7));
#elif defined(__arm__)
    asm volatile (".syntax unified                          \n"
                  /* copy 8-bit value to all 4 bytes in word */
#if __CORTEX_M < 3
                  /* Cortex-M0 does not support flexible 2nd operand */
                  "   uxtb %[val], %[val]                   \n"
                  "   lsls  r4, %[val], #8                  \n"
                  "   orrs %[val], %[val],r4                \n"
                  "   lsls  r4, %[val], #16                 \n"
                  "   orrs %[val], %[val], r4               \n"
#else
                  "   uxtb %[val], %[val]                   \n"
                  "   orr  %[val], %[val], %[val], lsl#8    \n"
                  "   orr  %[val], %[val], %[val], lsl#16   \n"
#endif
                  /* calculate max length of data that are word aligned */
                  "   adds r3, %[buf], %[len]               \n"
                  "   movs r4, #3                           \n"
                  "   ands r3, r3, r4                       \n"
                  "   subs r3, %[len], r3                   \n"
                  "   bmi  3f                               \n"
                  "   b    2f                               \n"
                  /* fill non-word aligned bytes at the end of buffer */
                  "1: subs %[len], #1                       \n"
                  "   strb %[val], [%[buf], %[len]]         \n"
                  "2: cmp  %[len], r3                       \n"
                  "   bne  1b                               \n"
                  /* fill all word aligned bytes */
                  "   b    2f                               \n"
                  "1: str  %[val], [%[buf], %[len]]         \n"
                  "2: subs %[len], #4                       \n"
                  "   bpl  1b                               \n"
                  "   adds %[len], #4                       \n"
                  /* fill remaining non-word aligned bytes */
                  "   b    3f                               \n"
                  "1: strb %[val], [%[buf], %[len]]         \n"
                  "3: subs %[len], #1                       \n"
                  "   bpl  1b                               \n"
                  : [buf] "+r" (q), [val] "+r" (c), [len] "+r" (n)
                  :
                  : "r3", "r4", "memory"
                 );
#else
	while (n--) {
		*q++ = c;
	}
#endif

	return dst;
}
