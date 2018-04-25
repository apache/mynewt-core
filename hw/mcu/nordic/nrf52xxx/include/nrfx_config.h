#ifndef NRFX_CONFIG_H__
#define NRFX_CONFIG_H__

#if NRF52
#include "nrfx52_config.h"
#elif NRF52840_XXAA
#include "nrfx52840_config.h"
#else
#error Unsupported chip selected
#endif

#endif // NRFX_CONFIG_H__
