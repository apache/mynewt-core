OS\_MEMPOOL\_SIZE
-----------------

.. code:: c

    OS_MEMPOOL_SIZE(n,blksize)

Calculates the number of os\_membuf\_t elements used by *n* blocks of
size *blksize* bytes.

Note that os\_membuf\_t is used so that memory blocks are aligned on
OS\_ALIGNMENT boundaries.

The *blksize* variable is the minimum number of bytes required for each
block; the actual block size is padded so that each block is aligned on
OS\_ALIGNMENT boundaries.

 #### Arguments

+-------------+-----------------------------------------+
| Arguments   | Description                             |
+=============+=========================================+
| n           | Number of elements                      |
+-------------+-----------------------------------------+
| blksize     | Size of an element is number of bytes   |
+-------------+-----------------------------------------+

Returned values
^^^^^^^^^^^^^^^

The number of os\_membuf\_t elements used by the memory pool. Note that
os\_membuf\_t is defined to be a unsigned, 32-bit integer when
OS\_ALIGNMENT is 4 and an unsigned, 64-bit integer when OS\_ALIGNMENT is
8.

 #### Notes OS\_MEMPOOL\_SIZE is a macro and not a function.

 #### Example

Here we define a memory buffer to be used by a memory pool using
OS\_MEMPOOL\_SIZE

.. code:: c

    #define NUM_BLOCKS      (16)
    #define BLOCK_SIZE      (32)

    os_membuf_t my_pool_memory[OS_MEMPOOL_SIZE(NUM_BLOCKS, BLOCK_SIZE)]
