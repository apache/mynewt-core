zephyr_library_sources(memfault_platform_debug_log.c)
zephyr_library_sources(memfault_platform_core.c)
zephyr_library_sources(memfault_zephyr_ram_regions.c)
zephyr_library_sources(memfault_platform_lock.c)
zephyr_library_sources(memfault_platform_coredump_regions.c)

# Allow users to Kconfig memfault logging as a backend as needed. See
# memfault/sdk/embedded/ports/zephyr/Kconfig and adjust MEMFAULT_LOGGING_XXX.
zephyr_library_sources_ifdef(CONFIG_MEMFAULT_LOGGING_ENABLE memfault_logging.c)
zephyr_library_sources_ifdef(CONFIG_MEMFAULT_HTTP_ENABLE memfault_platform_http.c)
zephyr_library_sources_ifdef(CONFIG_MEMFAULT_SHELL memfault_demo_cli.c)
zephyr_library_sources_ifdef(CONFIG_MEMFAULT_RAM_BACKED_COREDUMP memfault_platform_ram_backed_coredump.c)
zephyr_library_sources_ifdef(CONFIG_MEMFAULT_ROOT_CERT_STORAGE_TLS_CREDENTIAL_STORAGE memfault_tls_root_cert_storage.c)

if (NOT CONFIG_MEMFAULT_METRICS_TIMER_CUSTOM)
  zephyr_library_sources(memfault_platform_metrics.c)
endif()

if (NOT CONFIG_MEMFAULT_SOFTWARE_WATCHDOG_CUSTOM)
  zephyr_library_sources(memfault_software_watchdog.c)
endif()

if (CONFIG_MEMFAULT_CACHE_FAULT_REGS)
  # We need to wrap z_arm_fault() so we can get a copy
  # of the fault registers before Zephyr consumes and
  # clears the CFSR. See ports/zephyr/common/memfault_platform_core.c.
  zephyr_ld_options(-Wl,--wrap=z_arm_fault)
endif()

zephyr_library_sources_ifdef(CONFIG_MEMFAULT_HTTP_PERIODIC_UPLOAD memfault_http_periodic_upload.c)

zephyr_include_directories(.)
