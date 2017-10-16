newt resign-image 
------------------

Sign or re-sign an existing image file.

Usage:
^^^^^^

.. code-block:: console

        newt resign-image <image-file> [signing-key [key-id]][flags]

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

Changes the signature of an existing image file. To sign an image,
specify a .pem file for the ``signing-key`` and an optional ``key-id``.
``key-id`` must be a value between 0-255. If a signing key is not
specified, the command strips the current signature from the image file.

A new image header is created. The rest of the image is byte-for-byte
equivilent to the original image.

Warning: The image hash will change if you change the key-id or the type
of key used for signing.

Examples
^^^^^^^^

+----------------+--------------------------+--------------------+
| Sub-command    | Usage                    | Explanation        |
+================+==========================+====================+
| newt           | Signs the                |
| resign-image   | ``bin/targets/myble/app/ |
| bin/targets/my | apps/bletiny/bletiny.img |
| ble/app/apps/b | ``                       |
| letiny/bletiny | file with the            |
| .img           | private.pem key.         |
| private.pem    |                          |
+----------------+--------------------------+--------------------+
| newt           | Strips the current       |                    |
| resign-image   | signature from           |                    |
| bin/targets/my | ``bin/targets/myble/app/ |                    |
| ble/app/apps/b | apps/bletiny/bletiny.img |                    |
| letiny/bletiny | ``                       |                    |
| .img           | file.                    |                    |
+----------------+--------------------------+--------------------+
