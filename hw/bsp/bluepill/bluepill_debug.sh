#!/bin/sh

# This script attaches a gdb session to a Mynewt image running on your BSP.

# If your BSP uses JLink, a good example script to copy is:
#     repos/apache-mynewt-core/hw/bsp/nrf52dk/nrf52dk_debug.sh
#
# If your BSP uses OpenOCD, a good example script to copy is:
#     repos/apache-mynewt-core/hw/bsp/rb-nano2/rb-nano2_debug.sh

. $CORE_PATH/hw/scripts/openocd.sh

FILE_NAME=$BIN_BASENAME.elf
CFG="-s $BSP_PATH -f $BSP_PATH/bluepill.cfg"
# Exit openocd when gdb detaches.
EXTRA_JTAG_CMD="$EXTRA_JTAG_CMD; stm32f1x.cpu configure -event gdb-detach {if {[stm32f1x.cpu curstate] eq \"halted\"} resume;shutdown}"

openocd_debug