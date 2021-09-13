function(mflt_esp32_component_get_target var component_dir)
    set(${var} ${COMPONENT_LIB} PARENT_SCOPE)
endfunction()

set(MEMFAULT_ESP_IDF_VERSION_SPECIFIC_REQUIRES espcoredump)
