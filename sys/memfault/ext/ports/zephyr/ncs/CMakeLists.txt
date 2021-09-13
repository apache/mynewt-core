if(CONFIG_MEMFAULT_NRF_CONNECT_SDK)

  if (${KERNEL_VERSION_MAJOR} EQUAL 2)
    if (${KERNEL_VERSION_MINOR} LESS_EQUAL 3)
      if (CONFIG_MEMFAULT_HTTP_MAX_POST_SIZE EQUAL 0)
        message(FATAL_ERROR "CONFIG_MEMFAULT_HTTP_MAX_POST_SIZE=256 must be added to your prj.conf"
          " when using an nRF Connect SDK version older than v1.4")
      endif()
    endif()
  endif()

  add_subdirectory(src)
  zephyr_include_directories(include)

endif()
