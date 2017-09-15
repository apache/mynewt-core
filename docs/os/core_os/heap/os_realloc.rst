os\_realloc
-----------

.. code:: c

    void *os_realloc(void *ptr, size_t size)

Tries to resize previously allocated memory block, and returns pointer
to resized memory. ptr can be NULL, in which case the call is similar to
calling *os\_malloc()*.

Arguments
^^^^^^^^^

+-------------+------------------------------------------+
| Arguments   | Description                              |
+=============+==========================================+
| ptr         | Pointer to previously allocated memory   |
+-------------+------------------------------------------+
| size        | New size to adjust the memory block to   |
+-------------+------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

NULL: size adjustment was not successful. ptr: pointer to new start of
memory block

Notes
^^^^^

Example
^^^^^^^

.. code:: c

    <Insert the code snippet here>
