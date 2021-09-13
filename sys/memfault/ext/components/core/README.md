# core

Common code that is used by all other components.

## Application/platform-specific customizations

### Device information

The file `include/memfault/platform/device_info.h` contains the functions that
the Memfault SDK will call to get the unique device identifier (device serial),
software type, software version, hardware version, etc. The provided platform
reference implementation usually uses something like the WiFi MAC or chip ID as
base for the unique device identifier and uses preprocessor variables to get the
firmware and hardware versions (with fallbacks to hard-coded values, i.e.
`"xyz-main"` as software type `"1.0.0"` as software version and `"xyz-proto"` as
hardware version).

[To learn more about the concepts software type, software version, hardware version, etc. take a look at this documentation](https://mflt.io/36NGGgi).

Please change `memfault_platform_device_info.c` in ways that makes sense for
your project.

### Core features

- Build ID
- Packetizing and serializing data used for sending dat to the Memfault cloud.
- Event storage for metrics, heartbeat, reboot reasons and trace events. RAM
  based events can optionally be persisted to NVRAM via user-installed
  callbacks.
- Logging support provides a lightweight logging infrastructure with level
  filtering and the ability to write log entries quickly with user-installable
  callback support to drain log entries to slower mediums like NVRAM or serial
  devices. Information stored in the log module will be uploaded to Memfault in
  the event of a crash. Stored logs will also be uploaded after
  `memfault_log_trigger_collection()` is called.
- Reboot (reset) tracking captures a reset cause in a section of RAM that is not
  modified by any firmware during reset and rebooting.
- Memfault assert macro that can halt if the debugger is attached and call a
  user function if defined.
- Trace event capturing

### Storage allocations

- `memfault_events_storage_boot()` to reserve storage for events.
- `memfault_reboot_tracking_boot()` points to a memory region that is not
  touched by any firmware except for reboot tracking API functions. Reboot info
  can be pushed to the event storage for packetization to be sent to Memfault.

# `memfault_trace_event_boot()`: not an allocation but connects a trace event context into the event storge allocation.

- `memfault_log_boot()` to reserve storage for the logging module. The logging
  storage gets sent to Memfault as part of the core dump data if enabled.
