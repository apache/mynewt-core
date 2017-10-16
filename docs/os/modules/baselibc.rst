Baselibc
========

Baselibc is a very simple libc for embedded systems geared primarily for
32-bit microcontrollers in the 10-100kB memory range. The library of
basic system calls and facilities compiles to less than 5kB total on
Cortex-M3, and much less if some functions aren't used.

The code is based on klibc and tinyprintf modules, and licensed under
the BSD license.

Baselibc comes from https://github.com/PetteriAimonen/Baselibc.git

Description
~~~~~~~~~~~

Mynewt OS can utilize libc which comes with compiler (e.g. newlib
bundled with some binary distributions of arm-none-eabi-gcc). However,
you may choose to replace the libc with baselibc for a reduced image
size. Baselibc optimizes for size rather than performance, which is
usually a more important goal in embedded environments.

How to switch to baselibc
~~~~~~~~~~~~~~~~~~~~~~~~~

In order to switch from using libc to using baselibc you have to add the
baselibc pkg as a dependency in the project pkg. Specifying this
dependency ensures that the linker first looks for the functions in
baselibc before falling back to libc while creating the executable. For
example, project ``boot`` uses baselibc. Its project description file
``boot.yml`` looks like the following:

``no-highlight    project.name: boot    project.identities: bootloader    project.pkgs:        - libs/os        - libs/bootutil        - libs/nffs        - libs/console/stub        - libs/util        - libs/baselibc``

List of Functions
~~~~~~~~~~~~~~~~~

Documentation for libc functions is available from multiple places. One
example are the on-line manual pages at
`https://www.freebsd.org/cgi/man.cgi <#https://www.freebsd.org/cgi/man.cgi>`__.

baselibc supports most libc functionality; malloc(), printf-family,
string handling, and conversion routines.

There is some functionality which is not available, e.g. support for
floating point numbers, and limited support for 'long long'.
