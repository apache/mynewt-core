# Zephyr Specific Port

## Overview

This directory contains an implementation of the dependency functions needed to
integrate the Memfault SDK into the Zephyr RTOS.

The instructions below assume you have an environment already setup for building
and flashing a Zephyr application. If you do not, see the official
[getting started guide](https://docs.zephyrproject.org/2.0.0/getting_started/index.html#build-hello-world).

## Directories

The subdirectories within the folder are titled to align with the Zephyr release
the porting files were tested against. If no breaking API changes have been made
within Zephyr and a different release, the port may work there as well

- [v2.0](https://github.com/zephyrproject-rtos/zephyr/tree/v2.0-branch)
- [v1.14.1](https://github.com/memfault/zephyr/tree/v1.14-branch)

## Integrating SDK

1. Apply .patch in release directory to Zephyr Kernel

```
$ cd $ZEPHYR_ROOT_DIR/
$ git apply $MEMFAULT_SDK_ROOT/ports/zephyr/[v1.14|v2.0]/zephyr-integration.patch
```

2. Clone (or symlink) memfault-firmware-sdk in Zephyr Project

```
$ cd $ZEPHYR_ROOT_DIR/ext/lib/memfault
$ git clone https://github.com/memfault/memfault-firmware-sdk.git
```

3. Implement device-specific dependencies.

```
void memfault_platform_get_device_info(sMemfaultDeviceInfo *info) {
  *info = (sMemfaultDeviceInfo) {
    .device_serial = "DEMOSERIAL",
    .software_type = "zephyr-main",
    .software_version = "1.15.0",
    .hardware_version = "disco_l475_iot1",
  };
}
```

```
sMfltHttpClientConfig g_mflt_http_client_config = {
  .api_key = "<YOUR PROJECT KEY HERE>",
};
```

## Demo

An example integration and instructions can be found for the STM32L4 in
`$MEMFAULT_SDK_ROOT/examples/zephyr/README.md`
