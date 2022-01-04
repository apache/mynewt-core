Bootloader
==========

The "bootloader" is the code that loads the Mynewt OS image into memory
and conducts some checks before allowing the OS to be run. It manages
images for the embedded system and upgrades of those images using
protocols over various interfaces (e.g. serial, BLE, etc.). Typically,
systems with bootloaders have at least two program images coexisting on
the same microcontroller, and hence must include branch code that
performs a check to see if an attempt to update software is already
underway and manage the progress of the process.

The bootloader in the Apache Mynewt project verifies the cryptographic
signature of the firmware image before running it. It maintains a
detailed status log for each stage of the boot process. 

The "secure bootloader" should be placed in protected memory on a given
microcontroller.

The Apache Mynewt bootloader is the foundation of `MCUboot <https://github.com/runtimeco/mcuboot/>`_, 
a secure bootloader for 32-bit MCUs that has been ported to other Operating Systems as well. 

.. contents::
  :local:
  :depth: 2

The Mynewt bootloader comprises two packages:

-  The bootutil library (boot/bootutil)
-  The boot application (apps/boot)

The Mynewt code is thus structured so that the generic bootutil library
performs most of the functions of a boot loader. The final step of
actually jumping to the main image is kept out of the bootutil library.
This last step should instead be implemented in an architecture-specific
project. Boot loader functionality is separated in this manner for the
following two reasons:

1. By keeping architecture-dependent code separate, the bootutil library
   can be reused among several boot loaders.
2. By excluding the last boot step from the library, the bootloader can
   be unit tested since a library can be unit tested but an applicant
   can't.

Limitations
~~~~~~~~~~~

The boot loader currently only supports images with the following
characteristics:

-  Built to run from flash.
-  Build to run from a fixed location (i.e., position-independent).

Image Format
~~~~~~~~~~~~

The following definitions describe the image header format.

.. code:: c

    #define IMAGE_MAGIC                 0x96f3b83c
    #define IMAGE_MAGIC_NONE            0xffffffff

    struct image_version {
        uint8_t iv_major;
        uint8_t iv_minor;
        uint16_t iv_revision;
        uint32_t iv_build_num;
    };

    /** Image header.  All fields are in little endian byte order. */
    struct image_header {
        uint32_t ih_magic;
        uint16_t ih_tlv_size; /* Trailing TLVs */
        uint8_t  ih_key_id;
        uint8_t  _pad1;
        uint16_t ih_hdr_size;
        uint16_t _pad2;
        uint32_t ih_img_size; /* Does not include header. */
        uint32_t ih_flags;
        struct image_version ih_ver;
        uint32_t _pad3;
    };

The ``ih_hdr_size`` field indicates the length of the header, and
therefore the offset of the image itself. This field provides for
backwards compatibility in case of changes to the format of the image
header.

The following are the image header flags available.

.. code:: c

    #define IMAGE_F_PIC                    0x00000001
    #define IMAGE_F_SHA256                 0x00000002 /* Image contains hash TLV */
    #define IMAGE_F_PKCS15_RSA2048_SHA256  0x00000004 /* PKCS15 w/RSA and SHA */
    #define IMAGE_F_ECDSA224_SHA256        0x00000008 /* ECDSA256 over SHA256 */
    #define IMAGE_F_NON_BOOTABLE           0x00000010
    #define IMAGE_HEADER_SIZE              32

Optional type-length-value records (TLVs) containing image metadata are
placed after the end of the image. For example, security data gets added
as a footer at the end of the image.

.. code:: c

    /** Image trailer TLV format. All fields in little endian. */
    struct image_tlv {
        uint8_t  it_type;   /* IMAGE_TLV_[...]. */
        uint8_t  _pad;
        uint16_t it_len     /* Data length (not including TLV header). */
    };

    /*
     * Image trailer TLV types.
     */
    #define IMAGE_TLV_SHA256            1   /* SHA256 of image hdr and body */
    #define IMAGE_TLV_RSA2048           2   /* RSA2048 of hash output */
    #define IMAGE_TLV_ECDSA224          3   /* ECDSA of hash output */

Flash Map
~~~~~~~~~

A Mynewt device's flash is partitioned according to its *flash map*. At
a high level, the flash map maps numeric IDs to *flash areas*. A flash
area is a region of disk with the following properties:

1. An area can be fully erased without affecting any other areas.
2. A write to one area does not restrict writes to other areas.

The boot loader uses the following flash areas:

.. code:: c

    #define FLASH_AREA_BOOTLOADER                    0
    #define FLASH_AREA_IMAGE_0                       1
    #define FLASH_AREA_IMAGE_1                       2
    #define FLASH_AREA_IMAGE_SCRATCH                 3

Image Slots
~~~~~~~~~~~

A portion of the flash memory is partitioned into two image slots: a
primary slot and a secondary slot. The boot loader will only run an
image from the primary slot, so images must be built such that they can
run from that fixed location in flash. If the boot loader needs to run
the image resident in the secondary slot, it must swap the two images in
flash prior to booting.

In addition to the two image slots, the boot loader requires a scratch
area to allow for reliable image swapping.

Boot States
~~~~~~~~~~~

Logically, you can think of a pair of flags associated with each image
slot: pending and confirmed. On startup, the boot loader determines the
state of the device by inspecting each pair of flags. These flags have
the following meanings:

-  pending: image gets tested on next reboot; absent subsequent confirm
   command, revert to original image on second reboot.
-  confirmed: always use image unless excluded by a test image.

In English, when the user wants to run the secondary image, they set the
pending flag for the second slot and reboot the device. On startup, the
boot loader will swap the two images in flash, clear the secondary
slot's pending flag, and run the newly-copied image in slot 0. This is a
temporary state; if the device reboots again, the boot loader swaps the
images back to their original slots and boots into the original image.
If the user doesn't want to revert to the original state, they can make
the current state permanent by setting the confirmed flag in slot 0.

Switching to an alternate image is a two-step process (set + confirm) to
prevent a device from becoming "bricked" by bad firmware. If the device
crashes immediately upon booting the second image, the boot loader
reverts to the working image, rather than repeatedly rebooting into the
bad image.

The following set of tables illustrate the three possible states that
the device can be in:

::

                   | slot-0 | slot-1 |
    ---------------+--------+--------|
           pending |        |        |
         confirmed |   X    |        |
    ---------------+--------+--------'
    Image 0 confirmed;               |
    No change on reboot              |
    ---------------------------------'

                   | slot-0 | slot-1 |
    ---------------+--------+--------|
           pending |        |   X    |
         confirmed |   X    |        |
    ---------------+--------+--------'
    Image 0 confirmed;               |
    Test image 1 on next reboot      |
    ---------------------------------'

                   | slot-0 | slot-1 |
    ---------------+--------+--------|
           pending |        |        |
         confirmed |        |   X    |
    ---------------+--------+--------'
    Testing image 0;                 |
    Revert to image 1 on next reboot |
    ---------------------------------'

Boot Vector
~~~~~~~~~~~

At startup, the boot loader determines which of the above three boot
states a device is in by inspecting the boot vector. The boot vector
consists of two records (called "image trailers"), one written at the
end of each image slot. An image trailer has the following structure:

::

     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    ~                       MAGIC (16 octets)                       ~
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    ~                                                               ~
    ~             Swap status (128 * min-write-size * 3)            ~
    ~                                                               ~
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   Copy done   |     0xff padding (up to min-write-sz - 1)     ~
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   Image OK    |     0xff padding (up to min-write-sz - 1)     ~
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

These records are at the end of each image slot. The offset immediately
following such a record represents the start of the next flash area.

Note: ``min-write-size`` is a property of the flash hardware. If the
hardware allows individual bytes to be written at arbitrary addresses,
then ``min-write-size`` is 1. If the hardware only allows writes at even
addresses, then ``min-write-size`` is 2, and so on.

The fields are defined as follows:

1. MAGIC: The following 16 bytes, written in host-byte-order:

   ::

       const uint32_t boot_img_magic[4] = {
           0xf395c277,
           0x7fefd260,
           0x0f505235,
           0x8079b62c,
       };

2. Swap status: A series of single-byte records. Each record corresponds
   to a flash sector in an image slot. A swap status byte indicate the
   location of the corresponding sector data. During an image swap,
   image data is moved one sector at a time. The swap status is
   necessary for resuming a swap operation if the device rebooted before
   a swap operation completed.

3. Copy done: A single byte indicating whether the image in this slot is
   complete (``0x01=done, 0xff=not done``).

4. Image OK: A single byte indicating whether the image in this slot has
   been confirmed as good by the user
   (``0x01=confirmed; 0xff=not confirmed``).

The boot vector records are structured around the limitations imposed by
flash hardware. As a consequence, they do not have a very intuitive
design, and it is difficult to get a sense of the state of the device
just by looking at the boot vector. It is better to map all the possible
vector states to the swap types (None, Test, Revert) via a set of
tables. These tables are reproduced below. In these tables, the
"pending" and "confirmed" flags are shown for illustrative purposes;
they are not actually present in the boot vector.

::

    State I
                     | slot-0 | slot-1 |
    -----------------+--------+--------|
               magic | Unset  | Unset  |
            image-ok | Any    | N/A    |
    -----------------+--------+--------'
             pending |        |        |
          confirmed  |   X    |        |
    -----------------+--------+--------'
     swap: none                        |
    -----------------------------------'


    State II
                     | slot-0 | slot-1 |
    -----------------+--------+--------|
               magic | Any    | Good   |
            image-ok | Any    | N/A    |
    -----------------+--------+--------'
             pending |        |   X    |
          confirmed  |   X    |        |
    -----------------+--------+--------'
     swap: test                        |
    -----------------------------------'


    State III
                     | slot-0 | slot-1 |
    -----------------+--------+--------|
               magic | Good   | Unset  |
            image-ok | 0xff   | N/A    |
    -----------------+--------+--------'
             pending |        |        |
          confirmed  |        |   X    |
    -----------------+--------+--------'
     swap: revert (test image running) |
    -----------------------------------'


    State IV
                     | slot-0 | slot-1 |
    -----------------+--------+--------|
               magic | Good   | Unset  |
            image-ok | 0x01   | N/A    |
    -----------------+--------+--------'
             pending |        |        |
          confirmed  |   X    |        |
    -----------------+--------+--------'
     swap: none (confirmed test image) |
    -----------------------------------'

High-level Operation
~~~~~~~~~~~~~~~~~~~~

With the terms defined, we can now explore the boot loader's operation.
First, a high-level overview of the boot process is presented. Then, the
following sections describe each step of the process in more detail.

Procedure:

::

    A. Inspect swap status region; is an interrupted swap is being resumed?
        Yes: Complete the partial swap operation; skip to step C.
        No: Proceed to step B.

    B. Inspect boot vector; is a swap requested?
        Yes.
            1. Is the requested image valid (integrity and security check)?
                Yes.
                    a. Perform swap operation.
                    b. Persist completion of swap procedure to boot vector.
                    c. Proceed to step C.
                No.
                    a. Erase invalid image.
                    b. Persist failure of swap procedure to boot vector.
                    c. Proceed to step C.
        No: Proceed to step C.

    C. Boot into image in slot 0.

Image Swapping
~~~~~~~~~~~~~~

The boot loader swaps the contents of the two image slots for two
reasons:

-  User has issued an "image test" operation; the image in slot-1 should
   be run once (state II).
-  Test image rebooted without being confirmed; the boot loader should
   revert to the original image currently in slot-1 (state III).

If the boot vector indicates that the image in the secondary slot should
be run, the boot loader needs to copy it to the primary slot. The image
currently in the primary slot also needs to be retained in flash so that
it can be used later. Furthermore, both images need to be recoverable if
the boot loader resets in the middle of the swap operation. The two
images are swapped according to the following procedure:

::

    1. Determine how many flash sectors each image slot consists of.  This
       number must be the same for both slots.
    2. Iterate the list of sector indices in descending order (i.e., starting
       with the greatest index); current element = "index".
        b. Erase scratch area.
        c. Copy slot1[index] to scratch area.
        d. Write updated swap status (i).

        e. Erase slot1[index]
        f. Copy slot0[index] to slot1[index]
            - If these are the last sectors (i.e., first swap being perfomed),
              copy the full sector *except* the image trailer.
            - Else, copy entire sector contents.
        g. Write updated swap status (ii).

        h. Erase slot0[index].
        i. Copy scratch area slot0[index].
        j. Write updated swap status (iii).

    3. Persist completion of swap procedure to slot 0 image trailer.

The additional caveats in step 2f are necessary so that the slot 1 image
trailer can be written by the user at a later time. With the image
trailer unwritten, the user can test the image in slot 1 (i.e.,
transition to state II).

The particulars of step 3 vary depending on whether an image is being
tested or reverted:

::

    * test:
        o Write slot0.copy_done = 1
        (should now be in state III)

    * revert:
        o Write slot0.magic = BOOT_MAGIC
        o Write slot0.copy_done = 1
        o Write slot0.image_ok = 1
        (should now be in state IV)

Swap Status
~~~~~~~~~~~

The swap status region allows the boot loader to recover in case it
restarts in the middle of an image swap operation. The swap status
region consists of a series of single-byte records. These records are
written independently, and therefore must be padded according to the
minimum write size imposed by the flash hardware. In the below figure, a
``min-write-size`` of 1 is assumed for simplicity. The structure of the
swap status region is illustrated below. In this figure, a
``min-write-size`` of 1 is assumed for simplicity.

::

     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |sec127,state 0 |sec127,state 1 |sec127,state 2 |sec126,state 0 |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |sec126,state 1 |sec126,state 2 |sec125,state 0 |sec125,state 1 |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |sec125,state 2 |                                               |
    +-+-+-+-+-+-+-+-+                                               +
    ~                                                               ~
    ~               [Records for indices 124 through 1              ~
    ~                                                               ~
    ~               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    ~               |sec000,state 0 |sec000,state 1 |sec000,state 2 |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

And now, in English...

Each image slot is partitioned into a sequence of flash sectors. If we
were to enumerate the sectors in a single slot, starting at 0, we would
have a list of sector indices. Since there are two image slots, each
sector index would correspond to a pair of sectors. For example, sector
index 0 corresponds to the first sector in slot 0 and the first sector
in slot 1. Furthermore, we impose a limit of 128 indices. If an image
slot consists of more than 128 sectors, the flash layout is not
compatible with this boot loader. Finally, reverse the list of indices
such that the list starts with index 127 and ends with 0. The swap
status region is a representation of this reversed list.

During a swap operation, each sector index transitions through four
separate states:

::

    0. slot 0: image 0,   slot 1: image 1,   scratch: N/A
    1. slot 0: image 0,   slot 1: N/A,       scratch: image 1 (1->s, erase 1)
    2. slot 0: N/A,       slot 1: image 0,   scratch: image 1 (0->1, erase 0)
    3. slot 0: image 1,   slot 1: image 0,   scratch: N/A     (s->0)

Each time a sector index transitions to a new state, the boot loader
writes a record to the swap status region. Logically, the boot loader
only needs one record per sector index to keep track of the current swap
state. However, due to limitations imposed by flash hardware, a record
cannot be overwritten when an index's state changes. To solve this
problem, the boot loader uses three records per sector index rather than
just one.

Each sector-state pair is represented as a set of three records. The
record values map to the above four states as follows

::

            | rec0 | rec1 | rec2
    --------+------+------+------
    state 0 | 0xff | 0xff | 0xff
    state 1 | 0x01 | 0xff | 0xff
    state 2 | 0x01 | 0x02 | 0xff
    state 3 | 0x01 | 0x02 | 0x03

The swap status region can accommodate 128 sector indices. Hence, the
size of the region, in bytes, is ``128 * min-write-size * 3``. The
number 128 is chosen somewhat arbitrarily and will likely be made
configurable. The only requirement for the index count is that is is
great enough to account for a maximum-sized image (i.e., at least as
great as the total sector count in an image slot). If a device's image
slots use less than 128 sectors, the first record that gets written will
be somewhere in the middle of the region. For example, if a slot uses 64
sectors, the first sector index that gets swapped is 63, which
corresponds to the exact halfway point within the region.

Reset Recovery
~~~~~~~~~~~~~~

If the boot loader resets in the middle of a swap operation, the two
images may be discontiguous in flash. Bootutil recovers from this
condition by using the boot vector to determine how the image parts are
distributed in flash.

The first step is determine where the relevant swap status region is
located. Because this region is embedded within the image slots, its
location in flash changes during a swap operation. The below set of
tables map boot vector contents to swap status location. In these
tables, the "source" field indicates where the swap status region is
located.

::

              | slot-0     | scratch    |
    ----------+------------+------------|
        magic | Good       | Any        |
    copy-done | 0x01       | N/A        |
    ----------+------------+------------'
    source: none                        |
    ------------------------------------'

              | slot-0     | scratch    |
    ----------+------------+------------|
        magic | Good       | Any        |
    copy-done | 0xff       | N/A        |
    ----------+------------+------------'
    source: slot 0                      |
    ------------------------------------'

              | slot-0     | scratch    |
    ----------+------------+------------|
        magic | Any        | Good       |
    copy-done | Any        | N/A        |
    ----------+------------+------------'
    source: scratch                     |
    ------------------------------------'

              | slot-0     | scratch    |
    ----------+------------+------------|
        magic | Unset      | Any        |
    copy-done | 0xff       | N/A        |
    ----------+------------+------------|
    source: varies                      |
    ------------------------------------+------------------------------+
    This represents one of two cases:                                  |
    o No swaps ever (no status to read, so no harm in checking).       |
    o Mid-revert; status in slot 0.                                    |
    -------------------------------------------------------------------'

If the swap status region indicates that the images are not contiguous,
bootutil completes the swap operation that was in progress when the
system was reset. In other words, it applies the procedure defined in
the previous section, moving image 1 into slot 0 and image 0 into slot
1. If the boot status file indicates that an image part is present in
the scratch area, this part is copied into the correct location by
starting at step e or step h in the area-swap procedure, depending on
whether the part belongs to image 0 or image 1.

After the swap operation has been completed, the boot loader proceeds as
though it had just been started.

Integrity Check
~~~~~~~~~~~~~~~

An image is checked for integrity immediately before it gets copied into
the primary slot. If the boot loader doesn't perform an image swap, then
it doesn't perform an integrity check.

During the integrity check, the boot loader verifies the following
aspects of an image:

-  32-bit magic number must be correct (0x96f3b83c).
-  Image must contain a SHA256 TLV.
-  Calculated SHA256 must matche SHA256 TLV contents.
-  Image *may* contain a signature TLV. If it does, its contents must be
   verifiable using a key embedded in the boot loader.

Image Signing and Verification
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

As indicated above, the final step of the integrity check is signature
verification. The boot loader can have one or more public keys embedded
in it at build time. During signature verification, the boot loader
verifies that an image was signed with a private key that corresponds to
one of its public keys. The image signature TLV indicates the index of
the key that is has been signed with. The boot loader uses this index to
identify the corresponding public key.

For information on embedding public keys in the boot loader, as well as
producing signed images, see `here <https://github.com/apache/mynewt-core/blob/master/boot/bootutil/signed_images.md>`_.

