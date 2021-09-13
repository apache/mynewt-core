# demo

Common code that is used by demo apps for the various platforms. See
documentation in header files for details on the API itself.

The component contains a collection of cli commands which can also be useful for
bringup and initial integrations. You can find the list of available cli
commands at [include/memfault/demo/cli.h](include/memfault/demo/cli.h).

CLI commands specific to an optional component are found in
`src/${component}/*.c`.If you are using our CMake or Make
[build system helpers](README.md#add-sources-to-build-system) and enable the
demo component, these commands will only be included if the component has been
selected.
