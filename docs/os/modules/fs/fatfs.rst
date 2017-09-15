The FAT File System
===================

Mynewt provides an implementation of the FAT filesystem which is
currently supported on MMC/SD cards.

Description
~~~~~~~~~~~

    File Allocation Table (FAT) is a computer file system architecture
    and a family of industry-standard file systems utilizing it. The FAT
    file system is a legacy file system which is simple and robust. It
    offers good performance even in lightweight implementations, but
    cannot deliver the same performance, reliability and scalability as
    some modern file systems.

Configuration
~~~~~~~~~~~~~

``fatfs`` configuration can be tweaked by editing
``fs/fatfs/include/fatfs/ffconf.h``. The current configuraton was chosen
to minimize memory use and some options address limitations existing in
the OS:

-  Write support is enabled by default (can be disabled to minimize
   memory use).
-  Long filename (up to 255) support is disabled.
-  When writing files, time/dates are not persisted due to current lack
   of a standard ``hal_rtc`` interface.
-  No unicode support. Vanilla config uses standard US codepage 437.
-  Formatting of new volumes is disabled.
-  Default number of volumes is configured to 1.

API
~~~

To include ``fatfs`` on a project just include it as a dependency in
your project:

::

    pkg.deps:
        - fs/fatfs

It can now be used through the standard file system abstraction
functions as described in `FS API </os/modules/fs/fs/fs#API>`__.

Example
^^^^^^^

An example of using ``fatfs`` on a MMC card is provided on the
`MMC </os/modules/drivers/mmc#Example>`__ documentation.
