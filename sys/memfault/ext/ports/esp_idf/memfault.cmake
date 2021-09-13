# Typically, the platform specific dependencies (e.g. memfault_platform_get_device_info()) that are
# not part of the esp_idf default port are added to the "main" component. However, the user
# of the port may also explicitly setting the MEMFAULT_PLATFORM_PORT_COMPONENTS prior to including this
# cmake helper:
#
# set(MEMFAULT_PLATFORM_PORT_COMPONENTS <component>)

if(NOT DEFINED MEMFAULT_PLATFORM_PORT_COMPONENTS)
set(MEMFAULT_PLATFORM_PORT_COMPONENTS main)
message(STATUS "MEMFAULT_PLATFORM_PORT_COMPONENTS not provided, using default ('${MEMFAULT_PLATFORM_PORT_COMPONENTS}')")
endif()

# Some esp32 ports (i.e amazon-freertos) do not make use of the esp_http_client component so we
# expose a way to disable it entirely
if(NOT DEFINED MEMFAULT_ESP_HTTP_CLIENT_ENABLE)
  set(MEMFAULT_ESP_HTTP_CLIENT_ENABLE 1)
endif()

# Note: We want to forward some settings to the "memfault" idf component but that takes
# place at a later stage in the build process not included directly in this path so we
# pass settings using environment variables
set(ENV{MEMFAULT_PLATFORM_PORT_COMPONENTS} ${MEMFAULT_PLATFORM_PORT_COMPONENTS})
set(ENV{MEMFAULT_ESP_HTTP_CLIENT_ENABLE} ${MEMFAULT_ESP_HTTP_CLIENT_ENABLE})
set(ENV{MEMFAULT_PLATFORM_EXTRA_INCLUDES} ${MEMFAULT_PLATFORM_EXTRA_INCLUDES})

# This will inform esp-idf to pick up the memfault component which includes the
# memfault-firmware-sdk as well as the esp-idf porting layer that is not application specific
set(EXTRA_COMPONENT_DIRS ${EXTRA_COMPONENT_DIRS} ${CMAKE_CURRENT_LIST_DIR})
