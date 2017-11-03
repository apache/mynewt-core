Flash Circular Buffer (FCB)
===========================

Flash circular buffer provides an abstration through which you can treat
flash like a FIFO. You append entries to the end, and read data from the
beginning.

Description
~~~~~~~~~~~

Elements in the flash contain the length of the element, the data within
the element, and checksum over the element contents.

Storage of elements in flash is done in a FIFO fashion. When user
requests space for the next element, space is located at the end of the
used area. When user starts reading, the first element served is the
oldest element in flash.

Elements can be appended to the end of the area until storage space is
exhausted. User has control over what happens next; either erase oldest
block of data, thereby freeing up some space, or stop writing new data
until existing data has been collected. FCB treats underlying storage as
an array of flash sectors; when it erases old data, it does this a
sector at a time.

Elements in the flash are checksummed. That is how FCB detects whether
writing element to flash completed ok. It will skip over entries which
don't have a valid checksum.

Usage
~~~~~

To add an element to circular buffer:

-  Call fcb\_append() to get the location where data can be written. If
   this fails due to lack of space, you can call fcb\_rotate() to make
   some. And then call fcb\_append() again.
-  Use flash\_area\_write() to write element contents.
-  Call fcb\_append\_finish() when done. This completes the entry by
   calculating the checksum.

To read contents of the circular buffer: \* Call fcb\_walk() with a
pointer to your callback function. \* Within callback function copy in
data from the element using flash\_area\_read(). You can tell when all
data from within a sector has been read by monitoring returned element's
area pointer. Then you can call fcb\_rotate(), if you're done with that
data.

Alternatively: \* Call fcb\_getnext() with 0 in element offset to get
the pointer to oldest element. \* Use flash\_area\_read() to read
element contents. \* Call fcb\_getnext() with pointer to current element
to get the next one. And so on.

Data structures
~~~~~~~~~~~~~~~

This data structure describes the element location in the flash. You
would use it figure out what parameters to pass to flash\_area\_read()
to read element contents. Or to flash\_area\_write() when adding a new
element.

.. code:: c

    struct fcb_entry {
        struct flash_area *fe_area;
        uint32_t fe_elem_off;
        uint32_t fe_data_off;
        uint16_t fe_data_len;
    };

+------------+----------------+
| Element    | Description    |
+============+================+
| fe\_area   | Pointer to     |
|            | info about the |
|            | flash sector.  |
|            | Pass this to   |
|            | flash\_area\_x |
|            | x()            |
|            | routines.      |
+------------+----------------+
| fe\_elem\_ | Byte offset    |
| off        | from the start |
|            | of the sector  |
|            | to beginning   |
|            | of element.    |
+------------+----------------+
| fe\_data\_ | Byte offset    |
| off        | from start of  |
|            | the sector to  |
|            | beginning of   |
|            | element data.  |
|            | Pass this to   |
|            | to             |
|            | flash\_area\_x |
|            | x()            |
|            | routines.      |
+------------+----------------+
| fe\_data\_ | Number of      |
| len        | bytes in the   |
|            | element.       |
+------------+----------------+

The following data structure describes the FCB itself. First part should
be filled in by the user before calling fcb\_init(). The second part is
used by FCB for its internal bookkeeping.

.. code:: c

    struct fcb {
        /* Caller of fcb_init fills this in */
        uint32_t f_magic;           /* As placed on the disk */
        uint8_t f_version;          /* Current version number of the data */
        uint8_t f_sector_cnt;       /* Number of elements in sector array */
        uint8_t f_scratch_cnt;      /* How many sectors should be kept empty */
        struct flash_area *f_sectors; /* Array of sectors, must be contiguous */

        /* Flash circular buffer internal state */
        struct os_mutex f_mtx;      /* Locking for accessing the FCB data */
        struct flash_area *f_oldest;
        struct fcb_entry f_active;
        uint16_t f_active_id;
        uint8_t f_align;            /* writes to flash have to aligned to this */
    };

+------------+----------------+
| Element    | Description    |
+============+================+
| f\_magic   | Magic number   |
|            | in the         |
|            | beginning of   |
|            | FCB flash      |
|            | sector. FCB    |
|            | uses this when |
|            | determining    |
|            | whether sector |
|            | contains valid |
|            | data or not.   |
+------------+----------------+
| f\_version | Current        |
|            | version number |
|            | of the data.   |
|            | Also stored in |
|            | flash sector   |
|            | header.        |
+------------+----------------+
| f\_sector\ | Number of      |
| _cnt       | elements in    |
|            | the f\_sectors |
|            | array.         |
+------------+----------------+
| f\_scratch | Number of      |
| \_cnt      | sectors to     |
|            | keep empty.    |
|            | This can be    |
|            | used if you    |
|            | need to have   |
|            | scratch space  |
|            | for garbage    |
|            | collecting     |
|            | when FCB fills |
|            | up.            |
+------------+----------------+
| f\_sectors | Array of       |
|            | entries        |
|            | describing     |
|            | flash sectors  |
|            | to use.        |
+------------+----------------+
| f\_mtx     | Lock           |
|            | protecting     |
|            | access to FCBs |
|            | internal data. |
+------------+----------------+
| f\_oldest  | Pointer to     |
|            | flash sector   |
|            | containing the |
|            | oldest data.   |
|            | This is where  |
|            | data is served |
|            | when read is   |
|            | started.       |
+------------+----------------+
| f\_active  | Flash location |
|            | where the      |
|            | newest data    |
|            | is. This is    |
|            | used by        |
|            | fcb\_append()  |
|            | to figure out  |
|            | where the data |
|            | should go to.  |
+------------+----------------+
| f\_active\ | Flash sectors  |
| _id        | are assigned   |
|            | ever-increasin |
|            | g              |
|            | serial         |
|            | numbers. This  |
|            | is how FCB     |
|            | figures out    |
|            | where oldest   |
|            | data is on     |
|            | system         |
|            | restart.       |
+------------+----------------+
| f\_align   | Some flashes   |
|            | have           |
|            | restrictions   |
|            | on alignment   |
|            | for writes.    |
|            | FCB keeps a    |
|            | copy of this   |
|            | number for the |
|            | flash here.    |
+------------+----------------+

List of Functions
~~~~~~~~~~~~~~~~~

The functions available in this OS feature are:

+------------+----------------+
| Function   | Description    |
+============+================+
| `fcb\_init | Initializes    |
|  <fcb_init | the FCB. After |
| .html>`__    | calling this,  |
|            | you can start  |
|            | reading/writin |
|            | g              |
|            | data from FCB. |
+------------+----------------+
| `fcb\_appe | Start writing  |
| nd <fcb_ap | a new element  |
| pend.html>`_ | to flash.      |
| _          |                |
+------------+----------------+
| `fcb\_appe | Finalizes the  |
| nd\_finish | write of new   |
|  <fcb_appe | element. FCB   |
| nd_finish. | computes the   |
| md>`__     | checksum over  |
|            | the element    |
|            | and updates it |
|            | in flash.      |
+------------+----------------+
| `fcb\_walk | Walks over all |
|  <fcb_walk | log entries in |
| .html>`__    | FCB.           |
+------------+----------------+
| `fcb\_getn | Fills given    |
| ext <fcb_g | FCB location   |
| etnext.html> | with           |
| `__        | information    |
|            | about next     |
|            | element.       |
+------------+----------------+
| `fcb\_rota | Erase the      |
| te <fcb_ro | oldest sector  |
| tate.html>`_ | in FCB.        |
| _          |                |
+------------+----------------+
| `fcb\_appe | If FCB uses    |
| nd\_to\_sc | scratch        |
| ratch <fcb | blocks, use    |
| _append_to | reserve blocks |
| _scratch.m | when FCB is    |
| d>`__      | filled.        |
+------------+----------------+
| `fcb\_is\_ | Returns 1 if   |
| empty <fcb | there are no   |
| _is_empty. | elements       |
| md>`__     | stored in FCB, |
|            | otherwise      |
|            | returns 0.     |
+------------+----------------+
| `fcb\_offs | Returns the    |
| et\_last\_ | offset of n-th |
| n <fcb_off | last element.  |
| set_last_n |                |
| .html>`__    |                |
+------------+----------------+
| `fcb\_clea | Wipes out all  |
| r <fcb_cle | data in FCB.   |
| ar.html>`__  |                |
+------------+----------------+
