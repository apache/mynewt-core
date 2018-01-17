Console
-------

The console is an operating system window where users interact with the
OS subsystems or a console application. A user typically inputs text
from a keyboard and reads the OS output text on a computer monitor. The
text is sent as a sequence of characters between the user and the OS.

.. contents::
  :local:
  :depth: 2

You can configure the console to communicate via a UART or the SEGGER
Real Time Terminal (RTT) . The ``CONSOLE_UART`` syscfg setting enables
the communication via a UART and is enabled by default. The
``CONSOLE_RTT`` setting enables the communication via the RTT and is
disabled by default. When the ``CONSOLE_UART`` setting is enabled, the
following settings apply:

-  ``CONSOLE_UART_DEV``: Specifies the UART device to use. The default
   is ``uart0``.
-  ``CONSOLE_UART_BAUD``: Specifies the UART baud rate. The default is
   115200.
-  ``CONSOLE_UART_FLOW_CONTROL``: Specifies the UART flow control. The
   default is ``UART_FLOW_CONTROL_NONE``.
-  ``CONSOLE_UART_TX_BUF_SIZE``: Specifies the transmit buffer size and
   must be a power of 2. The default is 32.

The ``CONSOLE_TICKS`` setting enables the console to print the current
OS ticks in each output line.

**Notes:**

-  SEGGER RTT support is not available in the Mynewt 1.0 console
   package.
-  The console package is initialized during system initialization
   (sysinit) so you do not need to initialize the console. However, if
   you use the Mynewt 1.0 console API to read from the console, you will
   need to call the :c:func:`console_init()` function to enable backward
   compatibility support.

Description
~~~~~~~~~~~

In the Mynewt OS, the console library comes in three versions:

-  The ``sys/console/full`` package implements the complete console
   functionality and API.
-  The ``sys/console/stub`` package implements stubs for the API.
-  The ``sys/console/minimal`` package implements minimal console
   functionality of reading from and writing to console. It implements
   the :c:func:`console_read()` and :c:func:`console_write()` functions and stubs
   for all the other console functions.

All the packages export the ``console`` API, and any package that uses
the console API must list ``console`` as a requirement its ``pkg.yml``
file:

.. code-block:: yaml

    pkg.name: sys/shell
    pkg.deps:
        - kernel/os
        - encoding/base64
        - time/datetime
        - util/crc
    pkg.req_apis:
        - console

The project ``pkg.yml`` file also specifies the version of the console
package to use.

Using the Full Console Package
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A project that requires the full console capability must list the ``sys/console/full`` package as a
dependency in its ``pkg.yml`` file.

An example is the ``slinky`` application. It requires the full console
capability and has the following ``pkg.yml`` file:

.. code-block:: yaml

    pkg.name: apps/slinky
    pkg.deps:
        - test/flash_test
        - mgmt/imgmgr
        - mgmt/newtmgr
        - mgmt/newtmgr/transport/nmgr_shell
        - kernel/os
        - boot/bootutil
        - sys/shell
        - sys/console/full
           ...
        - sys/id

Using the Stub Console Package
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A project that uses console stub API must list the ``sys/console/stub``
package as a dependency in its ``pkg.yml`` file.

Examples of when a project would use the console stubs might be:

-  A project may not have a physical console (e.g. a UART port to
   connect a terminal to) but may have a dependency on a package that
   has console capability.
-  A bootloader project where we want to keep the size of the image
   small. It includes the ``kernel/os`` package that can print out
   messages on a console (e.g. if there is a hard fault). However, we do
   not want to use any console I/O capability in this particular
   bootloader project to keep the size small.

The project would use the console stub API and has the following
``pkg.yml`` file:

Another example would be the bootloader project where we want to keep
the size of the image small. It includes the ``libs/os`` pkg that can
print out messages on a console (e.g. if there is a hard fault) and the
``libs/util`` pkg that uses full console (but only if SHELL is present
to provide a CLI). However, we do not want to use any console I/O
capability in this particular bootloader project to keep the size small.
We simply use the console stub instead, and the pkg.yml file for the
project boot pkg looks like the following:

.. code-block:: yaml

    pkg.name: apps/boot
    pkg.deps:
        - boot/bootutil
        - kernel/os
        - sys/console/stub

Using the Minimal Console Package
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

There might be projects that need to read and write data on a serial
connection but do not need the full console capability. An example might
be a project that supports serial image upgrade but does not need full
newtmgr capability. The project would use the console minimal API and
has the following ``pkg.yml`` file:

.. code-block:: yaml

    pkg.name: apps/boot
    pkg.type: app
    pkg.description: Boot loader application.
    pkg.author: "Apache Mynewt <dev@mynewt.apache.org>"
    pkg.homepage: "http://mynewt.apache.org/"
    pkg.keywords:
        - loader

    pkg.deps:
        - boot/bootutil
        - kernel/os
        - sys/console/stub

    pkg.deps.BOOT_SERIAL.OVERWRITE:
        - sys/console/minimal
        - boot/boot_serial

Output to the Console
^^^^^^^^^^^^^^^^^^^^^

You use the :c:func:`console_write()` function to write raw output and the
:c:func:`console_printf()` function to write a C-style formatted string to the
console.

Input from the Console
^^^^^^^^^^^^^^^^^^^^^^

The following syscfg settings control input from the console:

-  ``CONSOLE_INPUT``: Enables input from the console. The setting is
   enabled by default.
-  ``CONSOLE_ECHO``: Enables echoing of the received data back to the
   console. Echoing is enabled by default. Terminal programs expect
   this, and is a way for the user to know that the console is connected
   and responsive. You can also use the :c:func:`console_echo()` function to
   set echo on or off programatically.
-  ``CONSOLE_MAX_INPUT_LEN``: Specifies the maximum input line length.

The Mynewt 1.1 console package adds a new API for reading input data
from the console. The package supports backward compatibility for the
Mynewt 1.0 console API. The steps you use to receive data from the
console for each API version are provided below.

Mynewt 1.0 Console API
^^^^^^^^^^^^^^^^^^^^^^^^^

To use the Mynewt 1.0 console API for reading input from the console,
you perform the follow steps:

1. Call the :c:func:`console_init()` function and pass either a pointer to a
   callback function or NULL for the argument. The console calls this
   callback function, if specified, when it receives a full line of
   data.

2. Call the :c:func:`console_read()` function to read the input data.

**Note:** The ``CONSOLE_COMPAT`` syscfg setting must be set to 1 to
enable backward compatibility support. The setting is enabled by
default.

Mynewt 1.1 Console API
^^^^^^^^^^^^^^^^^^^^^^^^^

Mynewt 1.1 console API adds the
:c:func:`console_set_queues()`
function. An application or the package, such as the shell, calls this
function to specify two event queues that the console uses to manage
input data buffering and to send notification when a full line of data
is received. The two event queues are used as follows:

-  **avail_queue**: Each event in this queue indicates that a buffer is
   available for the console to use for buffering input data.

   The caller must initialize the avail_queue and initialize and add an
   :doc:`../../os/core_os/event_queue/event_queue` to the
   avail_queue before calling the :c:func:`console_set_queues()` function.
   The fields for the event should be set as follows:

   -  ``ev_cb``: Pointer to the callback function to call when a
      full line of data is received.
   -  ``ev_arg``: Pointer to a :c:data:`console_input` structure. This
      structure contains a data buffer to store the current input.

   The console removes an event from this queue and uses the
   :c:data:`console_input` buffer from this event to buffer the received
   characters until it receives a new line, '/n', character. When the
   console receives a full line of data, it adds this event to the
   **lines_queue**.

-  **lines_queue**: Each event in this queue indicates a full line of
   data is received and ready for processing. The console adds an event
   to this queue when it receives a full line of data. This event is the
   same event that the console removes from the avail_queue.

   The task that manages the lines_queue removes an event from the
   queue and calls the event callback function to process the input
   line. The event callback function must add the event back to the
   avail_queue when it completes processing the current input data, and
   allows the console to use the :c:data:`console_input` buffer set for this
   event to buffer input data.

   We recommend that you use the OS default queue for the lines_queue
   so that the callback is processed in the context of the OS main task.
   If you do not use the OS default event queue, you must initialize an
   event queue and create a task to process events from the queue.

   **Note**: If the callback function needs to read another line of
   input from the console while processing the current line, it may use
   the :c:func:`console_read()` function to read the next line of input from
   the console. The console will need another :c:data:`console_input` buffer
   to store the next input line, so two events, initialized with the
   pointers to the callback and the :c:data:`console_input` buffer, must be
   added to the avail_queue.

Here is a code excerpt that shows how to use the
:c:func:`console_set_queues()` function. The example adds one event to the
avail_queue and uses the OS default event queue for the lines_queue.

.. code-block:: c

    static void myapp_process_input(struct os_event *ev);

    static struct os_eventq avail_queue;

    static struct console_input myapp_console_buf;

    static struct os_event myapp_console_event = {
        .ev_cb = myapp_process_input,
        .ev_arg = &myapp_console_buf
    };

    /* Event callback to process a line of input from console. */
    static void
    myapp_process_input(struct os_event *ev)
    {
        char *line;
        struct console_input *input;

        input = ev->ev_arg;
        assert (input != NULL);

        line = input->line;
        /* Do some work with line */
             ....
        /* Done processing line. Add the event back to the avail_queue */
        os_eventq_put(&avail_queue, ev);
        return;
    }

    static void
    myapp_init(void)
    {
        os_eventq_init(&avail_queue);
        os_eventq_put(&avail_queue, &myapp_console_event);

        console_set_queues(&avail_queue, os_eventq_dflt_get());
    }

API
~~~

.. doxygenfile:: full/include/console/console.h
