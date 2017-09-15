 os\_malloc
-----------

.. code:: c

    void *os_malloc(size_t size)

Allocates *size* number of bytes from heap and returns a pointer to it.

Arguments
^^^^^^^^^

+-------------+-------------------------------+
| Arguments   | Description                   |
+=============+===============================+
| size        | Number of bytes to allocate   |
+-------------+-------------------------------+

Returned values
^^^^^^^^^^^^^^^

: pointer to memory allocated from heap. NULL: not enough memory
available.

Notes
^^^^^

*os\_malloc()* calls *malloc()*, which is provided by C-library. The
heap must be set up during platform initialization. Depending on which
C-library you use, you might have to do the heap setup differently. Most
often *malloc()* implementation will maintain a list of allocated and
then freed memory blocks. If user asks for memory which cannot be
satisfied from free list, they'll call platform's *sbrk()*, which then
tries to grow the heap.

Example
^^^^^^^

.. code:: c

        info = (struct os_task_info *) os_malloc(
                sizeof(struct os_task_info) * tcount);
        if (!info) {
            rc = -1;
            goto err;
        }
