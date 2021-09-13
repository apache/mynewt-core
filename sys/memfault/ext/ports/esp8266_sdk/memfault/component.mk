ifdef CONFIG_MEMFAULT

MEMFAULT_SDK_ROOT := ../../..

COMPONENT_SRCDIRS += \
   $(MEMFAULT_SDK_ROOT)/components/core/src \
   $(MEMFAULT_SDK_ROOT)/components/demo/src \
   $(MEMFAULT_SDK_ROOT)/components/http/src \
   $(MEMFAULT_SDK_ROOT)/components/metrics/src \
   $(MEMFAULT_SDK_ROOT)/components/panics/src \
   $(MEMFAULT_SDK_ROOT)/components/util/src


COMPONENT_ADD_INCLUDEDIRS += \
  $(MEMFAULT_SDK_ROOT)/components/include \
  $(MEMFAULT_SDK_ROOT)/ports/esp8266_sdk/memfault/include \
  $(MEMFAULT_SDK_ROOT)/ports/freertos/include \
  $(MEMFAULT_SDK_ROOT)/ports/include \
  config

# Notes:
#  1. We --wrap the panicHandler so we can install a coredump collection routine
#  2. We add the .o for the RLE encoder directly because components get built as
#     a static library and we want to guarantee the weak functions get loaded
COMPONENT_ADD_LDFLAGS +=					\
  -Wl,--wrap=panicHandler					\
  build/memfault/components/core/src/memfault_data_source_rle.o

CFLAGS += \
  -DMEMFAULT_EVENT_STORAGE_READ_BATCHING_ENABLED=1 \
  -DMEMFAULT_METRICS_USER_HEARTBEAT_DEFS_FILE=\"memfault_metrics_heartbeat_esp_port_config.def\"

else

# Disable Memfault support
COMPONENT_ADD_INCLUDEDIRS :=
COMPONENT_ADD_LDFLAGS :=
COMPONENT_SRCDIRS :=

endif # CONFIG_MEMFAULT
