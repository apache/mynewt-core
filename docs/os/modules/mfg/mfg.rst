Manufacturing Support
=====================

.. toctree::
    :hidden:

Description
~~~~~~~~~~~

An mfgimage is a binary that gets written to a Mynewt device at
manufacturing time.  Unlike a Mynewt target which corresponds to a
single executable image, an mfgimage represents the entire contents
of a flash device.


Definitions
~~~~~~~~~~~

=============  ============================  =======
Term           Long Name                     Meaning
=============  ============================  =======
Flashdev       Flash device                  A single piece of flash hardware. A typical device might contain two flashdevs: 1) internal flash, and 2) external SPI flash.
Mfgimage       Manufacturing image           A file with the entire contents of a single flashdev. At manufacturing time, a separate mfgimage is written to each of the device's flashdevs.
Boot Mfgimage  Boot manufacturing image      The mfgimage containing the boot loader; always written to internal flash.
MMR            Manufacturing Meta Region     A chunk of read-only data included in every mfgimage. Contains identifying information for the mfgimage and other data that stays with the device until end of life.
TLV            Type Length Value             A simple extensible means of representing data. Contains three fields: 1) type of data, 2) length of data, and 3) the data itself.
MfgID          Manufacturing ID              Identifies which set of mfgimages a device was built with.  Expressed as a list of SHA256 hashes.
=============  ============================  =======


Details
~~~~~~~

Typically, an mfgimage consists of:

* 1 boot loader.
* 1 or 2 Mynewt images.
* Extra configuration (e.g., a pre-populated ``sys/config`` region).

In addition, each mfgimage contains a manufacturing meta region (MMR).
The MMR consists of read-only data that resides in flash for the
lifetime of the device.  There is currently support for three MMR TLV
types:

* Hash of mfgimage
* Flash map
* Device / offset of next MMR

The manufacturing hash indicates which manufacuturing image a device
was built with.  A management system may need this information to
determine which images a device can be upgraded to, for example.  A
Mynewt device exposes its manufacturing hash via the ``id/mfghash``
config setting.

Since MMRs are not intended to be modified or erased, they must be placed in
unmodifiable areas of flash.  In the boot mfgimage, the MMR *must* be placed in
the flash area containing the boot loader.  For non-boot mfgimages, the MMR can go in any unused area in the relevant flashdev.

Manufacturing ID
~~~~~~~~~~~~~~~~

Each mfgimage has its own MMR containing a hash.

The MMR at the end of the boot mfgimage ("boot MMR") must be present. The boot
MMR indicates the flash locations of other MMRs via the ``mmr_ref`` TLV type.

At startup, the firmware reads the boot MMR. Next, it reads
any additional MMRs indicated by ``mmr_ref`` TLVs. An ``mmr_ref`` TLV contains
one field: The ID of the flash area where the next MMR is located.

After all MMRs have been read, the firmware populates the ``id/mfghash``
setting with a colon-separated list of hashes. By reading and parsing
this setting, a client can derive the full list of mfgimages that the
device was built with.

One important implication is that MMR areas should never be moved in a BSP's
flash map.  Such a change would produce firmware that is incompatible with
older devices.


MMR Structure
~~~~~~~~~~~~~

An MMR is always located at the end its flash area.  Any other placement is invalid.

An MMR has the following structure:

::

     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   TLV type    |   TLV size    | TLV data ("TLV size" bytes)   ~
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               ~
    ~                                                               ~
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   TLV type    |   TLV size    | TLV data ("TLV size" bytes)   ~
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               ~
    ~                                                               ~
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |          Region size          |    Version    | 0xff padding  |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                       Magic (0x3bb2a269)                      |
    +-+-+-+-+-+--+-+-+-+-+-+end of flash area-+-+-+-+-+-+-+-+-+-+-+-+


The number of TLVs is variable; two are shown above for illustrative
purposes.

**Fields:**

*<TLVs>*

1. TLV type: Indicates the type of data to follow.
2. TLV size: The number of bytes of data to follow.
3. TLV data: "TLV size" bytes of data.

*<Footer>*

4. Region size: The size, in bytes, of the entire manufacturing meta region; includes TLVs and footer.
5. Version: Manufacturing meta version number; always 0x02.
6. Magic: Indicates the presence of the manufacturing meta region.

API
---

.. doxygenfile:: sys/mfg/include/mfg/mfg.h
