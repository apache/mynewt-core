Logging
-------

Mynewt log package supports logging of information within a Mynewt
application. It allows packages to define their own log streams with
separate names. It also allows an application to control the output
destination of logs. ### Description

In the Mynewt OS, the log package comes in two versions:

-  The ``sys/log/full`` package implements the complete log
   functionality and API.

-  The ``sys/log/stub`` package implements stubs for the API.

Both packages export the ``log`` API, and any package that uses the log
API must list ``log`` as a requirement in its ``pkg.yml`` file as
follows:

.. code-block:: console

    pkg.req_apis:
        - log

 The application's ``pkg.yml`` file specifies the version of the log
package to use. A project that requires the full logging capability must
list the ``sys/log/full`` package as a dependency in its ``pkg.yml``
file:

.. code-block:: console

    pkg.deps:
        - sys/log/full

 You can use the ``sys/log/stub`` package if you want to build your
application without logging to reduce code size.

Syscfg Settings
^^^^^^^^^^^^^^^

The ``LOG_LEVEL`` syscfg setting allows you to specify the level of logs
to enable in your application. Only logs for levels higher or equal to
the value of ``LOG_LEVEL`` are enabled. The amount of logs you include
affects your application code size. ``LOG_LEVEL: 0`` specifies
LOG\_LEVEL\_DEBUG and includes all logs. You set ``LOG_LEVEL: 255`` to
disable all logging. The ``#defines`` for the log levels are specified
in the ``sys/log/full/include/log/log.h`` file. For example the
following setting corresponds to LOG\_LEVEL\_ERROR:

.. code-block:: console

    syscfg.vals:
        LOG_LEVEL: 3   

The ``LOG_LEVEL`` setting applies to all modules registered with the log
package.

 ### Log

Each log stream requires a ``log`` structure to define its logging
properties.

Log Handler
~~~~~~~~~~~

To use logs, a log handler that handles the I/O from the log is
required. The log package comes with three pre-built log handlers:

-  console -- streams log events directly to the console port. Does not
   support walking and reading.
-  cbmem -- writes/reads log events to a circular buffer. Supports
   walking and reading for access by newtmgr and shell commands.
-  fcb -- writes/reads log events to a `flash circular
   buffer </os/modules/fcb/fcb.html>`__. Supports walking and reading for
   access by newtmgr and shell commands.

In addition, it is possible to create custom log handlers for other
methods. Examples may include

-  Flash file system
-  Flat flash buffer
-  Streamed over some other interface

To use logging, you typically do not need to create your own log
handler. You can use one of the pre-built ones.

A package or an application must define a variable of type
``struct log`` and register a log handler for it with the log package.
It must call the ``log_register()`` function to specify the log handler
to use:

.. code:: c

    log_register(char *name, struct log *log, const struct log_handler *lh, void *arg, uint8_t level)

The parameters are:

-  ``name``- Name of the log stream.
-  ``log`` - Log instance to register,
-  ``lh`` - Pointer to the log handler. You can specify one of the
   pre-built ones:

   -  ``&log_console_handler`` for console
   -  ``&log_cbm_handler`` for circular buffer
   -  ``&log_fcb_handler`` for flash circular buffer

-  ``arg`` - Opaque argument that the specified log handler uses. The
   value of this argument depends on the log handler you specify:

   -  NULL for the ``log_console_handler``.
   -  Pointer to an initialized ``cbmem`` structure (see ``util/cbmem``
      package) for the ``log_cbm_handler``.
   -  Pointer to an initialized ``fcb_log`` structure (see ``fs/fcb``
      package) for the ``log_fcb_handler``.

Typically, a package that uses logging defines a global variable, such
as ``my_package_log``, of type ``struct log``. The package can call the
``log_register()`` function with default values, but usually an
application will override the logging properties and where to log to.
There are two ways a package can allow an application to override the
values:

-  Define system configuration settings that an application can set and
   the package can then call the ``log_register()`` function with the
   configuration values.
-  Make the ``my_package_log`` variable external and let the application
   call the ``log_register()`` function to specify a log handler for its
   specific purpose.

Configuring Logging for Packages that an Application Uses
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Here is an example of how an application can set the log handlers for
the logs of the packages that the application includes.

In this example, the ``package1`` package defines the variable
``package1_log`` of type ``struct log`` and externs the variable.
Similarly, the ``package2`` package defines the variable
``package2_log`` and externs the variable. The application sets logs for
``package1`` to use console and sets logs for ``package2`` to use a
circular buffer.

.. code:: c

    #include <package1/package1.h>
    #include <package2/package2.h>
    #include <util/cbmem.h>

    #include <log/log.h>

    static uint32_t cbmem_buf[MAX_CBMEM_BUF];
    static struct cbmem cbmem;


    void app_log_init(void)
    {


       
        log_register("package1_log", &package1_log, &log_console_handler, NULL, LOG_SYSLEVEL);

        cbmem_init(&cbmem, cbmem_buf, MAX_CBMEM_BUF);
        log_register("package2_log", &package2_log, &log_cbmem_handler, &cbmem, LOG_SYSLEVEL);

    }

Implementing a Package that Uses Logging
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This example shows how a package logs to console. The package registers
default logging properties to use the console, but allows an application
to override the values. It defines the ``my_package_log`` variable and
makes it external so an application can override log handler.

Make the ``my_package_log`` variable external:

.. code:: c

    /* my_package.h*/

    /* pick a unique name here */
    extern struct log my_package_log;

Define the ``my_package_log`` variable and register the console log
handler:

.. code:: c

    /* my_package.c */

    struct log my_package_log;

    {
        ...

        /* register my log with a name to the system */
        log_register("log", &my_package_log, &log_console_handler, NULL, LOG_LEVEL_DEBUG);

        LOG_DEBUG(&my_package_log, LOG_MODULE_DEFAULT, "bla");
        LOG_DEBUG(&my_package_log, LOG_MODULE_DEFAULT, "bab");
    }

Log API and Log Levels
~~~~~~~~~~~~~~~~~~~~~~

For more information on the ``log`` API and log levels, see the
``sys/log/full/include/log/log.h`` header file.
