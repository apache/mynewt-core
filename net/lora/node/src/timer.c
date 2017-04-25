#include "os/os.h"
#include "node/timer.h"
#include "node/utilities.h"

uint32_t
TimerGetElapsedTime(uint32_t savedTime)
{ 
    return savedTime - os_cputime_get32();
}

uint32_t
TimerGetFutureTime(uint32_t eventInFuture)
{
    return os_cputime_get32() + eventInFuture;
}
