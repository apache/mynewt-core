Using HAL in Your Libraries
===========================

This page describes the recommended way to implement libraries that
utilize HAL functionality.

An example of the GPIO HAL being used by a driver for a UART bitbanger
that programs the start bit, data bits, and stop bit can be seen in
`hw/drivers/uart/uart\_bitbang/src/uart\_bitbang.c <https://github.com/apache/incubator-mynewt-core/blob/master/hw/drivers/uart/uart_bitbang/src/uart_bitbang.c>`__

An example of the flash HAL being used by a file sytem can be seen in
`fs/nffs/src/nffs\_flash.c <https://github.com/apache/incubator-mynewt-core/blob/master/fs/nffs/src/nffs_flash.c>`__.
