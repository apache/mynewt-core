# Make sure RAM based storage regions are not initialized on boot
# by placing them in special linker sections
zephyr_linker_sources(NOINIT memfault-no-init.ld)

zephyr_linker_sources(SECTIONS memfault-build-id.ld)

# Override the default Zephyr setting which disables the GNU Build ID
#   https://github.com/zephyrproject-rtos/zephyr/blob/d7ee114106eab485688223d97a49813d33b4cf21/cmake/linker/ld/target_base.cmake#L16
zephyr_ld_options("-Wl,--build-id")
