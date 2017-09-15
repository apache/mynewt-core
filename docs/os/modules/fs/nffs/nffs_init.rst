nffs\_init
----------

.. code:: c

    int nffs_init(void)

Initializes internal nffs memory and data structures. This must be
called before any nffs operations are attempted.

Returned values
^^^^^^^^^^^^^^^

-  0 on success
-  `FS error code <../fs/fs_return_codes.html>`__ on failure

Header file
^^^^^^^^^^^

.. code:: c

    #include "nffs/nffs.h"
