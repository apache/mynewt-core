newt mfg 
---------

Commands to create, build, and upload manufacturing image.

Usage:
^^^^^^

.. code-block:: console

        newt mfg [flags]
        newt mfg [command]

Available Commands:
^^^^^^^^^^^^^^^^^^^

.. code-block:: console

        create      Create a manufacturing flash image
        deploy      Build and upload a manufacturing image (build + load)
        load        Load a manufacturing flash image onto a device

Global Flags:
^^^^^^^^^^^^^

.. code-block:: console

        -h, --help              Help for newt commands
        -j, --jobs int          Number of concurrent build jobs (default 8)
        -l, --loglevel string   Log level (default "WARN")
        -o, --outfile string    Filename to tee output to
        -q, --quiet             Be quiet; only display error output
        -s, --silent            Be silent; don't output anything
        -v, --verbose           Enable verbose output when executing commands

Description
^^^^^^^^^^^

+----------------+---------------------------+
| Sub-command    | Explanation               |
+================+===========================+
| create         | A manufacturing image     |
|                | specifies 1) a boot       |
|                | loader target, and 2) one |
|                | or more image targets.    |
|                | Assuming the              |
|                | manufacturing entity has  |
|                | been created and defined  |
|                | in the                    |
|                | ``mfgs/<mfg image name>/` |
|                | `                         |
|                | package (see Examples     |
|                | below), this command      |
|                | collects the              |
|                | manufacturing related     |
|                | files in the newly        |
|                | created                   |
|                | ``bin/mfgs/<mfg image nam |
|                | e>``                      |
|                | directory. The collection |
|                | includes the image file,  |
|                | the hex file, and the     |
|                | manifests with the image  |
|                | build time, version,      |
|                | manufacturing package     |
|                | build time, image ID (or  |
|                | hash) etc. It is          |
|                | essentially a snapshot of |
|                | the image data and        |
|                | metadata uploaded to the  |
|                | device flash at           |
|                | manufacturing time. Note  |
|                | that the command expects  |
|                | the targets and images to |
|                | have already been built   |
|                | using ``newt build`` and  |
|                | ``newt create-image``     |
|                | commands.                 |
+----------------+---------------------------+
| deploy         | A combination of build    |
|                | and load commands to put  |
|                | together and upload       |
|                | manufacturing image on to |
|                | the device.               |
+----------------+---------------------------+
| load           | Loads the manufacturing   |
|                | package onto to the flash |
|                | of the connected device.  |
+----------------+---------------------------+

Examples
^^^^^^^^

Suppose you have created two targets (one for the bootloader and one for
the ``blinky`` app).

.. code-block:: console

    $ newt target show
    targets/my_blinky_sim
        app=apps/blinky
        bsp=@apache-mynewt-core/hw/bsp/native
        build_profile=debug
    targets/rb_blinky
        app=apps/blinky
        bsp=@apache-mynewt-core/hw/bsp/rb-nano2
        build_profile=debug
    targets/rb_boot
        app=@apache-mynewt-core/apps/boot
        bsp=@apache-mynewt-core/hw/bsp/rb-nano2
        build_profile=optimized

Create the directory to hold the mfg packages.

::

    $ mkdir -p mfgs/rb_blinky_rsa

The ``rb_blinky_rsa`` package needs a pkg.yml file. In addition it is
needs a mfg.yml file to specify the two constituent targets. An example
of each file is shown below.

::

    $  more mfgs/rb_blinky_rsa/pkg.yml 
    pkg.name: "mfgs/rb_blinky_rsa"
    pkg.type: "mfg"
    pkg.description: 
    pkg.author: 
    pkg.homepage: 

::

    $  more mfgs/rb_blinky_rsa/mfg.yml 
    mfg.bootloader: 'targets/rb_boot'
    mfg.images:
        - 'targets/rb_blinky'

Build the bootloader and app images.

::

    $ newt build rb_boot
    $ newt create-image rb_blinky 0.0.1

Run the ``newt mfg create`` command to collect all the manufacturing
snapshot files.

::

    $ newt mfg create rb_blinky_rsa
    Creating a manufacturing image from the following files:
    <snip>
    Generated the following files:
    <snip>
    $
