Enabling The Console and Shell for Blinky
-----------------------------------------

This tutorial shows you how to add the Console and Shell to the Blinky
application and interact with it over a serial line connection.

Prerequisites
~~~~~~~~~~~~~

-  Work through one of the Blinky Tutorials to create and build a Blinky
   application for one of the boards.
-  Have a `serial setup </os/get_started/serial_access.html>`__.

Use an Existing Project
~~~~~~~~~~~~~~~~~~~~~~~

Since all we're doing is adding the shell and console capability to
blinky, we assume that you have worked through at least some of the
other tutorials, and have an existing project. For this example, we'll
be modifying the `blinky on nrf52 <./nRF52.html>`__ project to enable the
shell and console connectivity. You can use blinky on a different board.

Modify the Dependencies and Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Modify the package dependencies in your application target's ``pkg.yml``
file as follows:

-  Add the shell package: ``@apache-mynewt-core/sys/shell``.
-  Replace the ``@apache-mynewt-core/sys/console/stub`` package with the
   ``@apache-mynewt-core/sys/console/full`` package.

   **Note**: If you are using version 1.1 or lower of blinky, the
   ``@apache-mynewt-core/sys/console/full`` package may be already
   listed as a dependency.

The updated ``pkg.yml`` file should have the following two lines:

.. code-block:: console

    pkg.deps:
        - "@apache-mynewt-core/sys/console/full"
        - "@apache-mynewt-core/sys/shell"

This lets the newt system know that it needs to pull in the code for the
console and the shell.

Modify the system configuration settings to enable Shell and Console
ticks and prompt. Add the following to your application target's
``syscfg.yml`` file:

.. code-block:: console

    syscfg.vals:
        # Enable the shell task.
        SHELL_TASK: 1
        SHELL_PROMPT_MODULE: 1

Use the OS Default Event Queue to Process Blinky Timer and Shell Events
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Mynewt creates a main task that executes the application ``main()``
function. It also creates an OS default event queue that packages can
use to queue their events. Shell uses the OS default event queue for
Shell events, and ``main()`` can process the events in the context of
the main task.

Blinky's main.c is very simple. It only has a ``main()`` function that
executes an infinite loop to toggle the LED and sleep for one second. We
will modify blinky:

-  To use os\_callout to generate a timer event every one second instead
   of sleeping. The timer events are added to the OS default event
   queue.
-  To process events from the OS default event queue inside the infinite
   loop in ``main()``.

This allows the main task to process both Shell events and the timer
events to toggle the LED from the OS default event queue.

Modify main.c
~~~~~~~~~~~~~

Initialize a os\_callout timer and move the toggle code from the while
loop in ``main()`` to the event callback function. Add the following
code above the ``main()`` function:

.. code:: c

    /* The timer callout */
    static struct os_callout blinky_callout;

    /*
     * Event callback function for timer events. It toggles the led pin.
     */
    static void
    timer_ev_cb(struct os_event *ev)
    {
        assert(ev != NULL);

        ++g_task1_loops;
        hal_gpio_toggle(g_led_pin);

        os_callout_reset(&blinky_callout, OS_TICKS_PER_SEC);
    }

    static void
    init_timer(void)
    {
        /*
         * Initialize the callout for a timer event.
         */
        os_callout_init(&blinky_callout, os_eventq_dflt_get(),
                        timer_ev_cb, NULL);

        os_callout_reset(&blinky_callout, OS_TICKS_PER_SEC);
    }

In ``main()``, add the call to the ``init_timer()`` function before the
while loop and modify the while loop to process events from the OS
default event queue:

\`\`\`c hl\_lines="15 17" int main(int argc, char \*\*argv) {

::

    int rc;

ifdef ARCH\_sim
===============

::

    mcu_sim_parse_args(argc, argv);

endif
=====

::

    sysinit();

    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);
    init_timer();
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;

}

\`\`\`

Build, Run, and Upload the Blinky Application Target
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We're not going to build the bootloader here since we are assuming that
you have already built and loaded it during previous tutorials.

We will use the ``newt run`` command to build and deploy our improved
blinky image. The run command performs the following tasks for us:

1. Builds a binary Mynewt executable
2. Wraps the executable in an image header and footer, turning it into a
   Mynewt image.
3. Uploads the image to the target hardware.
4. Starts a gdb process to remotely debug the Mynewt device.

Run the ``newt run nrf52_blinky 0`` command. The ``0`` is the version
number that should be written to the image header. Any version will do,
so we choose 0.

.. code-block:: console

    $ newt run nrf52_blinky 0

       ...

    Archiving util_mem.a
    Linking /home/me/dev/myproj/bin/targets/nrf52_blinky/app/apps/blinky/blinky.elf
    App image succesfully generated: /home/me/dev/myproj/bin/targets/nrf52_blinky/app/apps/blinky/blinky.elf
    Loading app image into slot 1
    [/home/me/dev/myproj/repos/apache-mynewt-core/hw/bsp/nrf52dk/nrf52dk_debug.sh /home/me/dev/myproj/repos/apache-mynewt-core/hw/bsp/nrf52dk /home/me/dev/myproj/bin/targets/nrf52_blinky/app/apps/blinky]
    Debugging /home/me/dev/myproj/bin/targets/nrf52_blinky/app/apps/blinky/blinky.elf

Set Up a Serial Connection
~~~~~~~~~~~~~~~~~~~~~~~~~~

You'll need a Serial connection to see the output of your program. You
can reference the `Serial Port
Setup <../get_started/serial_access.html>`__ Tutorial for more information
on setting up your serial communication.

Communicate with the Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once you have a connection set up, you can connect to your device as
follows:

-  On Mac OS and Linux platforms, you can run
   ``minicom -D /dev/tty.usbserial-<port> -b 115200`` to connect to the
   console of your app. Note that on Linux, the format of the port name
   is ``/dev/ttyUSB<N>``, where N is a number.

-  On Windows, you can use a terminal application such as PuTTY to
   connect to the device.

If you located your port from a MinGW terminal, the port name format is
``/dev/ttyS<N>``, where ``N`` is a number. You must map the port name to
a Windows COM port: ``/dev/ttyS<N>`` maps to ``COM<N+1>``. For example,
``/dev/ttyS2`` maps to ``COM3``.

You can also use the Windows Device Manager to locate the COM port.

 To test and make sure that the Shell is running, first just hit :

.. code-block:: console

    004543 shell>

You can try some commands:

.. code-block:: console

    003005 shell> help
    003137 Available modules:
    003137 os
    003138 prompt
    003138 To select a module, enter 'select <module name>'.
    003140 shell> prompt
    003827 help
    003827 ticks                         shell ticks command
    004811 shell> prompt ticks off
    005770  Console Ticks off
    shell> prompt ticks on
    006404  Console Ticks on
    006404 shell>
