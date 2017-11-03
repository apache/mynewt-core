fs\_getpos
----------

.. code:: c

    uint32_t fs_getpos(const struct fs_file *file)

Retrieves the current read and write position of the specified open
file.

Arguments
^^^^^^^^^

+--------------+--------------------------------+
| *Argument*   | *Description*                  |
+==============+================================+
| file         | Pointer to the file to query   |
+--------------+--------------------------------+

Returned values
^^^^^^^^^^^^^^^

-  The file offset, in bytes

Notes
^^^^^

If a file is opened in append mode, its write pointer is always
positioned at the end of the file. Calling this function on such a file
only indicates the read position.

Header file
^^^^^^^^^^^

.. code:: c

    #include "fs/fs.h"
