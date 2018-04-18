Compile-Time Configuration and Initialization
-----------------------------------------------

.. toctree::
   :hidden:

   sysconfig_error

This guide describes how Mynewt manages system configuration and
initialization. It shows you how to tell Mynewt to use default or
customized values to initialize packages that you develop or use to
build a target. This guide:

-  Assumes you have read the
   :ref:`concepts` section that describes
   the Mynewt package hierarchy and its use of the ``pkg.yml`` and
   ``syscfg.yml`` files.
-  Assumes you have read the Mynewt :doc:`../../../newt/newt_operation` and are familiar with how newt
   determines package dependencies for your target build.
-  Covers only the system initialization for hardware independent
   packages. It does not cover the Board Support Package (BSP) and other
   hardware dependent system initialization.

.. contents::
   :local:
   :depth: 3

Mynewt defines several configuration parameters in the ``pkg.yml`` and
``syscfg.yml`` files. The newt tool uses this information to:

-  Generate a system initialization function that calls all the
   package-specific system initialization functions.
-  Generate a system configuration header file that contains all the
   package configuration settings and values.
-  Display the system configuration settings and values in the
   ``newt target config`` command.

The benefits with this approach include:

-  Allows Mynewt developers to reuse other packages and easily change
   their configuration settings without updating source or header files
   when implementing new packages.
-  Allows application developers to easily view the system configuration
   settings and values and determine the values to override for a target
   build.

System Configuration Setting Definitions and Values
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A package can optionally:

-  Define and expose the system configuration settings to allow other
   packages to override the default setting values.
-  Override the system configuration setting values defined by the
   packages that it depends on.

You use the ``defs`` parameter in a ``syscfg.yml`` file to define the
system configuration settings for a package. ``defs`` is a mapping (or
associative array) of system configuration setting definitions. It has
the following syntax:

.. code-block:: yaml

    syscfg.defs:
        PKGA_SYSCFG_NAME1:
           description:
           value:
           type:
           restrictions:
        PKGA_SYSCFG_NAME2:
           description:
           value:
           type:
           restrictions:

Each setting definition consists of the following key-value mapping:

-  A setting name for the key, such as ``PKGA_SYSCFG_NAME1`` in the
   syntax example above. **Note:** A system configuration setting name
   must be unique. The newt tool aborts the build when multiple packages
   define the same setting.
-  A mapping of fields for the value. Each field itself is a key-value
   pair of attributes. The field keys are ``description``, ``value``,
   ``type``, and ``restrictions``. They are described in following
   table:

   ============  ===========
   Field         Description
   ============  ===========
   description   Describes the usage for the setting. This field is optional.

   value         Specifies the default value for the setting. This field is required. The
                 value depends on the type that you specify and can be an empty string.

   type          Specifies the data type for the value field. This field is optional. You
                 can specify one of three types:

                 ``raw``:
                   The ``value`` data is uninterpreted. This is the default ``type``.

                 ``task_priority``:
                   Specifies a Mynewt task priority number. The task
                   priority number assigned to each setting must be unique and between 0
                   and 239. value can be one of the following:

                   A number between 0 and 239 - The task priority number to use for the
                   setting.

                   ``any`` - Specify ``any`` to have newt automatically assign a priority for the
                   setting.
                   newt alphabetically orders all system configuration settings of this
                   type and assigns the next highest available task priority number to each
                   setting.

                 ``flash_owner``:
                   Specifies a flash area. The value should be the name of a
                   flash area defined in the BSP flash map for your target board.

   restrictions    Specifies a list of restrictions on the setting value. **This field is
                   optional**. You can specify two formats:

                   ``$notnull``:
                     Specifies that the setting cannot have the empty string for a
                     value. It essentially means that an empty string is not a sensible value
                     and a package must override it with an appropriate value.

                   ``expression``:
                     Specifies a boolean expression of the form
                     ``[!]&ltrequired-setting>[if &ltbase-value>]``

                   Examples:

                   ``restrictions: !LOG_FCB`` - When this setting is enabled, ``LOG_FCB`` must be
                   disabled.

                   ``restrictions: LOG_FCB if 0`` - When this setting is disabled, ``LOG_FCB``
                   must be enabled.

   ============  ===========


Examples of Configuration Settings
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Example 1
`````````

The following example is an excerpt from the
``sys/log/full`` package ``syscfg.yml`` file. It defines the
``LOG_LEVEL`` configuration setting to specify the log level and the
``LOG_NEWTMGR`` configuration setting to specify whether to enable or
disable the newtmgr logging feature.

.. code-block:: yaml

    syscfg.defs:
        LOG_LEVEL:
            description: 'Log Level'
            value: 0
            type: raw

           ...

        LOG_NEWTMGR:
            description: 'Enables or disables newtmgr command tool logging'
            value: 0

Example 2
`````````

The following example is an excerpt from the
``net/nimble/controller`` package ``syscfg.yml`` file. It defines the
``BLE_LL_PRIO`` configuration setting with a ``task_priority`` type and
assigns task priority 0 to the BLE link layer task.

.. code-block:: yaml

    syscfg.defs:
        BLE_LL_PRIO:
            description: 'BLE link layer task priority'
            type: 'task_priority'
            value: 0

Example 3
`````````

The following example is an excerpt from the ``fs/nffs``
package ``syscfg.yml`` file.

.. code-block:: yaml

    syscfg.defs:
        NFFS_FLASH_AREA:
            description: 'The flash area to use for the Newtron Flash File System'
            type: flash_owner
            value:
            restrictions:
                - $notnull

It defines the ``NFFS_FLASH_AREA`` configuration setting with a
``flash_owner`` type indicating that a flash area needs to be specified
for the Newtron Flash File System. The flash areas are typically defined
by the BSP in its ``bsp.yml`` file. For example, the ``bsp.yml`` for
nrf52dk board (``hw/bsp/nrf52dk/bsp.yml``) defines an area named
``FLASH_AREA_NFFS``:

.. code-block:: yaml

        FLASH_AREA_NFFS:
            user_id: 1
            device: 0
            offset: 0x0007d000
            size: 12kB

The ``syscfg.yml`` file for the same board
(``hw/bsp/nrf52dk/syscfg.yml``) specifies that the above area be used
for ``NFFS_FLASH_AREA``.

.. code-block:: yaml

    syscfg.vals:
        CONFIG_FCB_FLASH_AREA: FLASH_AREA_NFFS
        REBOOT_LOG_FLASH_AREA: FLASH_AREA_REBOOT_LOG
        NFFS_FLASH_AREA: FLASH_AREA_NFFS
        COREDUMP_FLASH_AREA: FLASH_AREA_IMAGE_1

Note that the ``fs/nffs/syscfg.yml`` file indicates that the
``NFFS_FLASH_AREA`` setting cannot be a null string; so a higher
priority package must set a non-null value to it. That is exactly what
the BSP package does. For more on priority of packages in setting
values, see the next section.

Overriding System Configuration Setting Values
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A package may use the ``vals`` parameter in its ``syscfg.yml`` file to
override the configuration values defined by other packages. This
mechanism allows:

-  Mynewt developers to implement a package and easily override the
   system configuration setting values that are defined by the packages
   it depends on.
-  Application developers to easily and cleanly override default
   configuration settings in a single place and build a customized
   target. You can use the ``newt target config show <target-name>``
   command to check all the system configuration setting definitions and
   values in your target to determine the setting values to override.
   See `newt target </newt/command_list/newt_target.html>`__.

``vals`` specifies the mappings of system configuration setting
name-value pairs as follows:

.. code-block:: yaml

    syscfg.vals:
        PKGA_SYSCFG_NAME1: VALUE1
        PKGA_SYSCFG_NAME2: VALUE2
                  ...
        PKGN_SYSCFG_NAME1: VALUEN

**Note**: The newt tool ignores overrides of undefined system
configuration settings.

Resolving Override Conflicts
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The newt tool uses package priorities to determine whether a package can
override a value and resolve conflicts when multiple packages override
the same system configuration setting. The following rules apply:

-  A package can only override the default values of system
   configuration settings that are defined by lower priority packages.
-  When packages with different priorities override the same system
   configuration setting value, newt uses the value from the highest
   priority package.
-  Packages of equal priority cannot override the same system
   configuration setting with different values. newt aborts the build
   unless a higher priority package also overrides the value.

The following package types are listed from highest to lowest priority:

-  Target
-  App
-  unittest - A target can include either an app or unit test package,
   but not both.
-  BSP
-  Lib - Includes all other system level packages such as os, lib, sdk,
   and compiler. (Note that a Lib package cannot override other Lib
   package settings.)

It is recommended that you override defaults at the target level instead
of updating individual package ``syscfg.yml`` files.

Examples of Overrides
^^^^^^^^^^^^^^^^^^^^^

Example 4
``````````

The following example is an excerpt from the
``apps/slinky`` package ``syscfg.yml`` file. The application package
overrides, in addition to other packages, the ``sys/log/full`` package
system configuration settings defined in `Example 1`_. It changes the
LOG_NEWTMGR system configuration setting value from ``0`` to ``1``.

.. code-block:: yaml

    syscfg.vals:
        # Enable the shell task.
        SHELL_TASK: 1

           ...

        # Enable newtmgr commands.
        STATS_NEWTMGR: 1
        LOG_NEWTMGR: 1

Example 5
`````````

The following example are excerpts from the
``hw/bsp/native`` package ``bsp.yml`` and ``syscfg.yml`` files. The
package defines the flash areas for the BSP flash map in the ``bsp.yml``
file, and sets the ``NFFS_FLASH_AREA`` configuration setting value to
use the flash area named ``FLASH_AREA_NFFS`` in the ``syscfg.yml`` file.

.. code-block:: yaml

    bsp.flash_map:
        areas:
            # System areas.
            FLASH_AREA_BOOTLOADER:
                device: 0
                offset: 0x00000000
                size: 16kB

                 ...

            # User areas.
            FLASH_AREA_REBOOT_LOG:
                user_id: 0
                device: 0
                offset: 0x00004000
                size: 16kB
            FLASH_AREA_NFFS:
                user_id: 1
                device: 0
                offset: 0x00008000
                size: 32kB


    syscfg.vals:
        NFFS_FLASH_AREA: FLASH_AREA_NFFS

Generated syscfg.h and Referencing System Configuration Settings
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The newt tool processes all the package ``syscfg.yml`` files and
generates the ``bin/<target-path>/generated/include/syscfg/syscfg.h``
include file with ``#define`` statements for each system configuration
setting defined. Newt creates a ``#define`` for a setting name as
follows:

-  Adds the prefix ``MYNEWT_VAL_``.
-  Replaces all occurrences of "/", "-", and " " in the setting name
   with "_".
-  Converts all characters to upper case.

For example, the #define for my-config-name setting name is
MYNEWT_VAL_MY_CONFIG_NAME.

Newt groups the settings in ``syscfg.h`` by the packages that defined
them. It also indicates the package that changed a system configuration
setting value.

You must use the ``MYNEWT_VAL()`` macro to reference a #define of a
setting name in your header and source files. For example, to reference
the ``my-config-name`` setting name, you use
``MYNEWT_VAL(MY_CONFIG_NAME)``.

**Note:** You only need to include ``syscfg/syscfg.h`` in your source
files to access the ``syscfg.h`` file. The newt tool sets the correct
include path to build your target.

Example of syscfg.h and How to Reference a Setting Name
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Example 6**: The following example are excerpts from a sample
``syscfg.h`` file generated for an app/slinky target and from the
``sys/log/full`` package ``log.c`` file that shows how to reference a
setting name.

The ``syscfg.h`` file shows the ``sys/log/full`` package definitions and
also indicates that ``app/slinky`` changed the value for the
``LOG_NEWTMGR`` settings.

.. code-block:: cpp

    /**
     * This file was generated by Apache Newt version: 1.0.0-dev
     */

    #ifndef H_MYNEWT_SYSCFG_
    #define H_MYNEWT_SYSCFG_

    /**
     * This macro exists to ensure code includes this header when needed.  If code
     * checks the existence of a setting directly via ifdef without including this
     * header, the setting macro will silently evaluate to 0.  In contrast, an
     * attempt to use these macros without including this header will result in a
     * compiler error.
     */
    #define MYNEWT_VAL(x)                           MYNEWT_VAL_ ## x

    /* ... */

    /*** kernel/os */
    #ifndef MYNEWT_VAL_MSYS_1_BLOCK_COUNT
    #define MYNEWT_VAL_MSYS_1_BLOCK_COUNT (12)
    #endif

    #ifndef MYNEWT_VAL_MSYS_1_BLOCK_SIZE
    #define MYNEWT_VAL_MSYS_1_BLOCK_SIZE (292)
    #endif

    /* ... */

    /*** sys/log/full */

    #ifndef MYNEWT_VAL_LOG_LEVEL
    #define MYNEWT_VAL_LOG_LEVEL (0)
    #endif

    /* ... */

    /* Overridden by apps/slinky (defined by sys/log/full) */
    #ifndef MYNEWT_VAL_LOG_NEWTMGR
    #define MYNEWT_VAL_LOG_NEWTMGR (1)
    #endif

    #endif

The ``log_init()`` function in the ``sys/log/full/src/log.c`` file
initializes the ``sys/log/full`` package. It checks the ``LOG_NEWTMGR``
setting value, using ``MYNEWT_VAL(LOG_NEWTMGR)``, to determine whether
the target application has enabled the ``newtmgr log`` functionality. It
only registers the the callbacks to process the ``newtmgr log`` commands
when the setting value is non-zero.

.. code-block:: cpp

    void
    log_init(void)
    {
        int rc;

        /* Ensure this function only gets called by sysinit. */
        SYSINIT_ASSERT_ACTIVE();

        (void)rc;

        if (log_inited) {
            return;
        }
        log_inited = 1;

        /* ... */

    #if MYNEWT_VAL(LOG_NEWTMGR)
        rc = log_nmgr_register_group();
        SYSINIT_PANIC_ASSERT(rc == 0);
    #endif
    }

System Initialization
~~~~~~~~~~~~~~~~~~~~~

During system startup, Mynewt creates a default event queue and a main
task to process events from this queue. You can override the
``OS_MAIN_TASK_PRIO`` and ``OS_MAIN_TASK_STACK_SIZE`` setting values
defined by the ``kernel/os`` package to specify different task priority
and stack size values.

Your application's ``main()`` function executes in the context of the
main task and must perform the following:

-  At the start of ``main()``, call the Mynewt ``sysinit()`` function to
   initialize the packages before performing any other processing.
-  At the end of ``main()``, wait for and dispatch events from the
   default event queue in an infinite loop.

**Note:** You must include the ``sysinit/sysinit.h`` header file to
access the ``sysinit()`` function.

Here is an example of a ``main()`` function:

.. code-block:: cpp

    int
    main(int argc, char **argv)
    {
        /* First, call sysinit() to perform the system and package initialization */
        sysinit();

        /* ... other application initialization processing ... */

        /*  Last, process events from the default event queue.  */
        while (1) {
           os_eventq_run(os_eventq_dflt_get());
        }
        /* main never returns */
    }

Specifying Package Initialization Functions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``sysinit()`` function calls the ``sysinit_app()`` function to
perform system initialization for the packages in the target. You can,
optionally, specify one or more package initialization functions that
``sysinit_app()`` calls to initialize a package.

A package initialization function must have the following prototype:

.. code-block:: cpp

    void init_func_name(void)

Package initialization functions are called in stages to ensure that
lower priority packages are initialized before higher priority packages.
A stage is an integer value, 0 or higher, that specifies when an
initialization function is called. Mynewt calls the package
initialization functions in increasing stage number order. The call
order for initialization functions with the same stage number depends on
the order the packages are processed, and you cannot rely on a specific
call order for these functions.

You use the ``pkg.init`` parameter in the ``pkg.yml`` file to specify an
initialization function and the stage number to call the function. You
can specify multiple initialization functions, with a different stage
number for each function, for the parameter values. This feature allows
packages with interdependencies to perform initialization in multiple
stages.

The ``pkg.init`` parameter has the following syntax in the ``pkg.yml``
file:

.. code-block:: yaml

    pkg.init:
        pkg_init_func1_name: pkg_init_func1_stage
        pkg_init_func2_name: pkg_init_func2_stage

                  ...

        pkg_init_funcN_name: pkg_init_funcN_stage

where ``pkg_init_func#_name`` is the C function name of an
initialization function, and ``pkg_init_func#_stage`` is an integer
value, 0 or higher, that indicates the stage when the
``pkg_init_func#_name`` function is called.

**Note:** The ``pkg.init_function`` and ``pkg.init_stage`` parameters
introduced in a previous release for specifying a package initialization
function and a stage number are deprecated and have been retained to
support the legacy format. They will not be maintained for future
releases and we recommend that you migrate to use the ``pkg.init``
parameter.

Generated sysinit_app() Function
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The newt tool processes the ``pkg.init`` parameters in all the
``pkg.yml`` files for a target, generates the ``sysinit_app()`` function
in the ``<target-path>/generated/src/<target-name>-sysinit_app.c`` file,
and includes the file in the build. Here is an example ``sysinit_app()``
function:

.. code-block:: cpp

    /**
     * This file was generated by Apache Newt (incubating) version: 1.0.0-dev
     */

    #if !SPLIT_LOADER

    void split_app_init(void);
    void os_pkg_init(void);
    void imgmgr_module_init(void);

    /* ... */

    void stats_module_init(void);

    void
    sysinit_app(void)
    {

        /*** Stage 0 */
        /* 0.0: kernel/os */
        os_pkg_init();

        /*** Stage 2 */
        /* 2.0: sys/flash_map */
        flash_map_init();

        /*** Stage 10 */
        /* 10.0: sys/stats/full */
        stats_module_init();

        /*** Stage 20 */
        /* 20.0: sys/console/full */
        console_pkg_init();

        /*** Stage 100 */
        /* 100.0: sys/log/full */
        log_init();
        /* 100.1: sys/mfg */
        mfg_init();

        /* ... */

        /*** Stage 300 */
        /* 300.0: sys/config */
        config_pkg_init();

        /*** Stage 500 */
        /* 500.0: sys/id */
        id_init();
        /* 500.1: sys/shell */
        shell_init();

        /* ... */

        /* 500.4: mgmt/imgmgr */
        imgmgr_module_init();

        /*** Stage 501 */
        /* 501.0: mgmt/newtmgr/transport/nmgr_shell */
        nmgr_shell_pkg_init();
    }
    #endif

Conditional Configurations
~~~~~~~~~~~~~~~~~~~~~~~~~~

You can use the system configuration setting values to conditionally
specify parameter values in ``pkg.yml`` and ``syscfg.yml`` files. The
syntax is:

.. code-block:: yaml

    parameter_name.PKGA_SYSCFG_NAME:
         parameter_value

This specifies that ``parameter_value`` is only set for
``parameter_name`` if the ``PKGA_SYSCFG_NAME`` configuration setting
value is non-zero. Here is an example from the ``libs/os`` package
``pkg.yml`` file:

.. code-block:: yaml

    pkg.deps:
        - sys/sysinit
        - util/mem

    pkg.deps.OS_CLI
        - sys/shell

This example specifies that the ``os`` package depends on the
``sysinit`` and ``mem`` packages, and also depends on the ``shell``
package when ``OS_CLI`` is enabled.

The newt tool aborts the build when it detects circular conditional
dependencies.
