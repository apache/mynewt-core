#include "hal/hal_system.h"
#include "testutil_priv.h"

void
tu_arch_restart(void)
{
    system_reset();
}
