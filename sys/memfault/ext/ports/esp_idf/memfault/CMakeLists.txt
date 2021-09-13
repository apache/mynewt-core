set(MEMFAULT_SDK_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)

list(APPEND MEMFAULT_COMPONENTS core util panics demo http metrics)
include(${MEMFAULT_SDK_ROOT}/cmake/Memfault.cmake)
memfault_library(${MEMFAULT_SDK_ROOT} MEMFAULT_COMPONENTS
  MEMFAULT_COMPONENTS_SRCS MEMFAULT_COMPONENTS_INC_FOLDERS ARCH_XTENSA)

# v4.0 and greater expose the IDF_VERSION in cmake. If we can't find
# it, we assume the end user is on v3.x

include($ENV{IDF_PATH}/tools/cmake/version.cmake OPTIONAL)

# Version 3.3.5 and later provide IDF_VERSION_MAJOR.
if(DEFINED IDF_VERSION_MAJOR)
  if(IDF_VERSION_MAJOR VERSION_EQUAL 4)
    set(MEMFAULT_ESP_IDF_PORT v4.x)
  elseif(IDF_VERSION_MAJOR VERSION_EQUAL 3)
    set(MEMFAULT_ESP_IDF_PORT v3.x)
  else()
    message(FATAL_ERROR "IDF_VERSION_MAJOR=${IDF_VERSION_MAJOR} provided is not currenly supported")
  endif()
else()
  message(STATUS "No IDF_VERSION_MAJOR provided, using esp-idf v3.x Memfault port")
  set(MEMFAULT_ESP_IDF_PORT v3.x)
endif()

# esp-idf version specific porting files
list(APPEND MEMFAULT_COMPONENTS_SRCS
  ${MEMFAULT_ESP_IDF_PORT}/memfault_esp_spi_flash.c
)

include(${CMAKE_CURRENT_LIST_DIR}/${MEMFAULT_ESP_IDF_PORT}/Memfault-esp-idf-compat.cmake)

# This directory holds ports that are common across v3.x and v4.x esp-idf releases
set(MEMFAULT_ESP_IDF_PORT_COMMON common)
list(APPEND MEMFAULT_COMPONENTS_SRCS
    ${MEMFAULT_ESP_IDF_PORT_COMMON}/memfault_fault_handler.c
    ${MEMFAULT_ESP_IDF_PORT_COMMON}/memfault_platform_core.c
    ${MEMFAULT_ESP_IDF_PORT_COMMON}/memfault_platform_coredump.c
    ${MEMFAULT_ESP_IDF_PORT_COMMON}/memfault_platform_debug_log.c
    ${MEMFAULT_ESP_IDF_PORT_COMMON}/memfault_platform_demo_cli_cmds.c
    ${MEMFAULT_ESP_IDF_PORT_COMMON}/memfault_platform_http_client.c
    ${MEMFAULT_ESP_IDF_PORT_COMMON}/memfault_platform_http_client_buffer.c
    ${MEMFAULT_ESP_IDF_PORT_COMMON}/memfault_platform_metrics.c
)

list(APPEND MEMFAULT_COMPONENTS_INC_FOLDERS
   include
   include/${MEMFAULT_ESP_IDF_PORT}
   $ENV{MEMFAULT_PLATFORM_EXTRA_INCLUDES}
)

# Register Memfault SDK Component
set(COMPONENT_SRCS ${MEMFAULT_COMPONENTS_SRCS})
set(COMPONENT_ADD_INCLUDEDIRS ${MEMFAULT_COMPONENTS_INC_FOLDERS})
list(APPEND COMPONENT_REQUIRES
    ${MEMFAULT_ESP_IDF_VERSION_SPECIFIC_REQUIRES}
    $ENV{MEMFAULT_PLATFORM_PORT_COMPONENTS}
    esp32
    freertos
    heap
    log
    soc
    spi_flash
    console
)

if($ENV{MEMFAULT_ESP_HTTP_CLIENT_ENABLE})
list(APPEND COMPONENT_REQUIRES esp_http_client)
endif()
register_component()

# sdk_overrides/esp_http_client.c needs the (private) esp_http header files:
get_filename_component(this_directory_name . ABSOLUTE DIRECTORY)
get_filename_component(this_directory_name ${this_directory_name} NAME)
mflt_esp32_component_get_target(this_component ${this_directory_name})

component_compile_options(-DMEMFAULT_ESP_HTTP_CLIENT_ENABLE=$ENV{MEMFAULT_ESP_HTTP_CLIENT_ENABLE})

# We will intercept the panic handlers enabled by CONFIG_ESP32_ENABLE_COREDUMP_TO_FLASH
# and run the Memfault Fault Handler instead.
#
# Benefits here are:
#   FreeRTOS task list is walked server side instead of on device (so you can get crash data even if the lists are corrupted)
#   Much more flexibility in debug information collected (e.g. all RAM, just the current stack trace, select stacks and variables)
#   Data can be posted directly from device to Memfault cloud for deduplication and analysis
target_link_libraries(${this_component} INTERFACE "-Wl,--wrap=esp_core_dump_to_flash -Wl,--wrap=esp_core_dump_init -Wl,--wrap=esp_core_dump_image_get")
