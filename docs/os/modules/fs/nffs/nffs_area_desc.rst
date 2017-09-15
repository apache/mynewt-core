struct nffs\_area\_desc
-----------------------

.. code:: c

    struct nffs_area_desc {
        uint32_t nad_offset;    /* Flash offset of start of area. */
        uint32_t nad_length;    /* Size of area, in bytes. */
        uint8_t nad_flash_id;   /* Logical flash id */
    };

Descriptor for a single nffs area. An area is a region of disk with the
following properties:

1. An area can be fully erased without affecting any other areas.
2. Writing to one area does not restrict writes to other areas.

**Regarding property 1:** Generally, flash hardware divides its memory
space into "blocks." When erasing flash, entire blocks must be erased in
a single operation; partial erases are not possible.

**Regarding property 2:** Furthermore, some flash hardware imposes a
restriction with regards to writes: writes within a block must be
strictly sequential. For example, if you wish to write to the first 16
bytes of a block, you must write bytes 1 through 15 before writing byte
16. This restriction only applies at the block level; writes to one
block have no effect on what parts of other blocks can be written.

Thus, each area must comprise a discrete number of blocks.

An array of area descriptors is terminated by an entry with a
*nad\_length* value of 0.

Notes
^^^^^

Typically, a product's flash layout is exposed via its BSP-specific
``bsp_flash_dev()`` function. This function retrieves the layout of the
specified flash device resident in the BSP. The result of this function
can then be converted into the ``struct nffs_area_desc[]`` that nffs
requires.

Header file
^^^^^^^^^^^

.. code:: c

    #include "nffs/nffs.h"
