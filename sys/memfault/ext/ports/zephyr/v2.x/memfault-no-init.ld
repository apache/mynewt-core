/**
 * Memfault stores some state across reboots. These regions must be placed in a region of RAM that
 * does not get initialized on bootup
 */
KEEP(*(*.mflt_coredump));
KEEP(*(*.mflt_reboot_info));
