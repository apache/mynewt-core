fs/fs Return Codes
------------------

Functions in ``fs/fs`` that indicate success or failure do so with the
following set of return codes:

+----------------+---------------------------------------------+
| Return code    | Description                                 |
+================+=============================================+
| FS\_EOK        | Success                                     |
+----------------+---------------------------------------------+
| FS\_ECORRUPT   | File system corrupt                         |
+----------------+---------------------------------------------+
| FS\_EHW        | Error accessing storage medium              |
+----------------+---------------------------------------------+
| FS\_EOFFSET    | Invalid offset                              |
+----------------+---------------------------------------------+
| FS\_EINVAL     | Invalid argument                            |
+----------------+---------------------------------------------+
| FS\_ENOMEM     | Insufficient memory                         |
+----------------+---------------------------------------------+
| FS\_ENOENT     | No such file or directory                   |
+----------------+---------------------------------------------+
| FS\_EEMPTY     | Specified region is empty (internal only)   |
+----------------+---------------------------------------------+
| FS\_EFULL      | Disk full                                   |
+----------------+---------------------------------------------+
| FS\_EUNEXP     | Disk contains unexpected metadata           |
+----------------+---------------------------------------------+
| FS\_EOS        | OS error                                    |
+----------------+---------------------------------------------+
| FS\_EEXIST     | File or directory already exists            |
+----------------+---------------------------------------------+
| FS\_EACCESS    | Operation prohibited by file open mode      |
+----------------+---------------------------------------------+
| FS\_EUNINIT    | File system not initialized                 |
+----------------+---------------------------------------------+

Header file
^^^^^^^^^^^

.. code:: c

    #include "fs/fs.h"
