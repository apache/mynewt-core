zephyr_library_sources(memfault_fault_handler.c)
zephyr_include_directories(.)

# Zephyr fatals for ARM Cortex-M's take the following path:
#
# frame 3 k_sys_fatal_error_handler    nrf/lib/fatal_error/fatal_error.c
#                                      (overrides weak impl in zephyr/kernel/fatal.c)
#  frame 2 z_fatal_error               zephyr/kernel/fatal.c
#   frame 1 z_arm_fault                zephyr/arch/arm/core/aarch32/cortex_m/fault.c
#    frame 0 z_arm_hard_fault          zephyr/arch/arm/core/aarch32/cortex_m/fault_s.S
#
# The nrf-connect-sdk does not expose a mechanism to override "k_sys_fatal_error_handler"
# and simply reboots the system so instead we intercept the frame above and install the
# Memfault fault handler which will collect a coredump.
target_link_libraries(app INTERFACE "-Wl,--wrap=z_fatal_error")

# We trace task creation so the task TCBs and stacks can be collected at the time
# of a crash and all thread backtraces can be made available in the Memfault UI.
#
# To do this we wrap the arch_new_thread() function call that is made from thread.c
#
# A nicer way to do this would be to make use of the "sys_trace_thread_create" macro.
# Unfortunately, to override the macro, one must patch the Zephyr RTOS today.
#   https://github.com/zephyrproject-rtos/zephyr/blob/390537b/include/tracing/tracing.h#L57-L61
target_link_libraries(app INTERFACE "-Wl,--wrap=arch_new_thread")
