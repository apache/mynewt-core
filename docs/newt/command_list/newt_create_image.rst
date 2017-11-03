newt create-image 
------------------

Create and sign an image by adding an image header to the binary file
created for a target. Version number in the header is set to <version>.
To sign an image provide a .pem file for the signing-key and an optional
key-id.

Usage:
^^^^^^

.. code-block:: console

        newt create-image <target-name> <version> [signing-key [key-id]][flags]

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

Adds an image header to the created binary file for the ``target-name``
target. The image version is set to ``version``. It creates a
``<app-name>.img`` file the image, where ``app-name`` is the value
specified in the target ``app`` variable, and stores the file in the
'/bin/targets/<target-name>/app/apps/<app-name>/' directory. It also
creates a ``<app-name>.hex`` file for the image in the same directory,
and adds the version, build id, image file name, and image hash to the
``manifest.json`` file that the ``newt build`` command created.

To sign an image, provide a .pem file for the ``signing-key`` and an
optional ``key-id``. ``key-id`` must be a value between 0-255.

Examples
^^^^^^^^

+----------------+--------------------------+--------------------+
| Sub-command    | Usage                    | Explanation        |
+================+==========================+====================+
| newt           | Creates an image for     |
| create-image   | target ``myble2`` and    |
| myble2 1.0.1.0 | assigns it version       |
|                | ``1.0.1.0``. For the     |
|                | following target         |
|                | definition:              |
|                | targets/myble2           |
|                | app=@apache-mynewt-core/ |
|                | apps/bletiny             |
|                | bsp=@apache-mynewt-core/ |
|                | hw/bsp/nrf52dk           |
|                | build\_profile=optimized |
|                | syscfg=STATS\_NAMES=1    |
|                | the                      |
|                | 'bin/targets/myble2/app/ |
|                | apps/bletiny/bletiny.img |
|                | '                        |
|                | and                      |
|                | 'bin/targets/myble2/app/ |
|                | apps/bletiny/bletiny.hex |
|                | '                        |
|                | files are created, and   |
|                | the manifest in          |
|                | 'bin/targets/myble2/app/ |
|                | apps/bletiny/manifest.js |
|                | on'                      |
|                | is updated with the      |
|                | image information.       |
+----------------+--------------------------+--------------------+
| newt           | Creates an image for     |
| create-image   | target ``myble2`` and    |
| myble2 1.0.1.0 | assigns it the version   |
| private.pem    | ``1.0.1.0``. Signs the   |
|                | image using private key  |
|                | specified by the         |
|                | private.pem file.        |
+----------------+--------------------------+--------------------+
