/*
 * memcmp.c
 */

#include <string.h>

int memcmp(const void *s1, const void *s2, size_t n)
{
    int d = 0;

#if defined(ARCH_cortex_m3) || defined(ARCH_cortex_m4) || defined(ARCH_cortex_m7)
    asm (".syntax unified                   \n"
         "       push  {r4, r5, r6}         \n"
         "       mov   r5, #0               \n"
         "       and   r6, r2, #0xfffffffc  \n"
         "       b     test1                \n"
         "loop1: ldr   r3, [r0, r5]         \n"
         "       ldr   r4, [r1, r5]         \n"
         "       cmp   r3, r4               \n"
         "       bne   res1                 \n"
         "       add   r5, #4               \n"
         "test1: cmp   r5, r6               \n"
         "       bne   loop1                \n"
         "       b     test2                \n"
         "res1:  rev   r3, r3               \n"
         "       rev   r4, r4               \n"
         "       subs  r3, r4               \n"
         "       ite   hi                   \n"
         "       movhi r3, #1               \n"
         "       movls r3, #-1              \n"
         "       b     done                 \n"
         "loop2: ldrb  r3, [r0, r5]         \n"
         "       ldrb  r4, [r1, r5]         \n"
         "       subs  r3, r4               \n"
         "       bne   done                 \n"
         "       add   r5, #1               \n"
         "test2: cmp   r5, r2               \n"
         "       bne   loop2                \n"
         "       mov   r3, #0               \n"
         "done:  mov   %[res], r3           \n"
         "       pop   {r4, r5, r6}         \n"
         : [res] "=r" (d)
        );
#else
	const unsigned char *c1 = s1, *c2 = s2;

	while (n--) {
		d = (int)*c1++ - (int)*c2++;
		if (d)
			break;
	}
#endif

	return d;
}
