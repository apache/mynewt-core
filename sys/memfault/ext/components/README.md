# Memfault 💥

## Components

This directory contains the Memfault components that fall into two categories:

- Main components
- Support components

A brief mention of each of the components is described here, however, see the
`README.md` in the respective directories for a more complete description of
each component. See also the online documentation at
[Components](https://github.com/memfault/memfault-firmware-sdk#components).

### Main Components

The main components are `panics` and `metrics`. These components provide the
high level features of the Memfault SDK to your product.

- panics - fault handling, coredump and reboot tracking and reboot loop
  detection API.
- metrics - used to monitor device health over time (i.e. connectivity, battery
  life, MCU resource utilization, hardware degradation, etc.)

### Support Components

The support components include `core`, `http`, `util` and `demo` which, except
for `demo`, are needed by the main components. The `demo` component is there to
help demonstrate how to use the Memfault SDK.

- core - common code that is used by all other components.
- demo - common code that is used by demo apps for the various platforms.
- http - http client API, to post coredumps and events directly to the Memfault
  service from devices.
- util - various utilities.
