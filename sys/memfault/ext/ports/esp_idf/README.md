# ESP32 Specific Port

## Overview

This directory contains an implementation of the dependency functions needed to
integrate the Memfault SDK into a ESP-IDF based project.

The port has been tested on major versions 3 and 4 of the ESP-IDF.

## Directories

The subdirectories within the folder are titled to align with the ESP-IDF
"component" structure.

```
├── README.md
├── memfault.cmake # cmake helper to include to add the memfault "component"
└── memfault
    ├── CMakeLists.txt
    ├── common # sourced common across all ESP-IDF versions
    ├── include
    ├── v3.x # ESP-IDF sources specific to major version 3
    └── v4.x # ESP-IDF sources specific to major version 4
```

## Integrating the SDK

A step-by-step integration guide can be found at https://mflt.io/esp-tutorial
