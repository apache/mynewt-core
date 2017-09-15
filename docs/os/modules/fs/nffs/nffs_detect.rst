nffs\_detect
------------

.. code:: c

    int nffs_detect(const struct nffs_area_desc *area_descs)

Searches for a valid nffs file system among the specified areas. This
function succeeds if a file system is detected among any subset of the
supplied areas. If the area set does not contain a valid file system, a
new one can be created via a separate call to nffs\_format().

Arguments
^^^^^^^^^

+------------------+----------------------------------+
| *Argument*       | *Description*                    |
+==================+==================================+
| area\_descs      | The set of areas to search. This |
|                  | array must be terminated with a  |
|                  | 0-length area.                   |
+------------------+----------------------------------+

Returned values
^^^^^^^^^^^^^^^

-  0 on success
-  FS\_ECORRUPT if no valid file system was detected
-  Other `FS error code <../fs/fs_return_codes.html>`__ on failure

Header file
^^^^^^^^^^^

.. code:: c

    #include "nffs/nffs.h"

Example
^^^^^^^

.. code:: c

    /*** hw/hal/include/hal/flash_map.h */

    /*
     * Flash area types
     */
    #define FLASH_AREA_BOOTLOADER           0
    #define FLASH_AREA_IMAGE_0              1
    #define FLASH_AREA_IMAGE_1              2
    #define FLASH_AREA_IMAGE_SCRATCH        3
    #define FLASH_AREA_NFFS                 4

.. code:: c

    /*** project/slinky/src/main.c */

    int
    main(int argc, char **argv)
    {
        int rc;
        int cnt;

        /* NFFS_AREA_MAX is defined in the BSP-specified bsp.h header file. */
        struct nffs_area_desc descs[NFFS_AREA_MAX];

        /* Initialize nffs's internal state. */
        rc = nffs_init();
        assert(rc == 0);

        /* Convert the set of flash blocks we intend to use for nffs into an array
         * of nffs area descriptors.
         */
        cnt = NFFS_AREA_MAX;
        rc = flash_area_to_nffs_desc(FLASH_AREA_NFFS, &cnt, descs);
        assert(rc == 0);

        /* Attempt to restore an existing nffs file system from flash. */
        if (nffs_detect(descs) == FS_ECORRUPT) {
            /* No valid nffs instance detected; format a new one. */
            rc = nffs_format(descs);
            assert(rc == 0);
        }
        /* [ ... ] */
    }
