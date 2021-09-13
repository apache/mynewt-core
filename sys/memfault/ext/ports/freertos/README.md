# FreeRTOS Specific Port

## Overview

This directory contains an implementation of Memfault dependency functions when
a platform is built on top of [FreeRTOS](https://www.freertos.org/).

## Usage

1. Add `$MEMFAULT_FIRMWARE_SDK/ports/freertos/include` to your projects include
   path
2. Prior to using the Memfault SDK in your code, initialize the FreeRTOS port.

```c
#include "memfault/ports/freertos.h"

void main(void) {

  memfault_freertos_port_boot();
}
```

## Heap Tracking

The memfault-firmware-sdk has a built in utility for tracking heap allocations to facilitate debug
of out of memory bugs. To enable, add the following to your `memfault_platform_config.h` file:

```c
#define MEMFAULT_FREERTOS_PORT_HEAP_STATS_ENABLE 1
#define MEMFAULT_COREDUMP_HEAP_STATS_LOCK_ENABLE 0
#define MEMFAULT_COREDUMP_COLLECT_HEAP_STATS 1
```
