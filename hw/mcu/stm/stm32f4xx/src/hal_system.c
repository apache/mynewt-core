#include "stm32f4xx/stm32f407xx.h"
#include "hal/hal_system.h"

void
system_reset(void)
{
    NVIC_SystemReset();
}
