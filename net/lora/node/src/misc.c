#include <stdlib.h>

#include "syscfg/syscfg.h"
#include "node/utilities.h"
#include "lora_priv.h"

int32_t
randr(int32_t min, int32_t max)
{
    return rand() % (max - min + 1) + min;
}

void
memcpyr( uint8_t *dst, const uint8_t *src, uint16_t size )
{
    dst = dst + ( size - 1 );
    while( size-- )
    {
        *dst-- = *src++;
    }
}

double
ceil(double d)
{
    int64_t i;

    i = d;
    if (d == i) {
        return i;
    }
    return i + 1;
}

double
floor(double d)
{
    return (int64_t)d;
}

double
round(double d)
{
    return (int64_t)(d + 0.5);
}
