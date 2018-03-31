#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H

#include "os/mynewt.h"

#define SEGGER_RTT_LOCK()   {                       \
                            int sr;                 \
                            OS_ENTER_CRITICAL(sr);
#define SEGGER_RTT_UNLOCK() OS_EXIT_CRITICAL(sr)    \
                            }

#endif
