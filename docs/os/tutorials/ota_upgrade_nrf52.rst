Over-the-Air Image Upgrade
--------------------------

Mynewt OS supports over-the-air image upgrades. This tutorial shows you
how to use the newtmgr tool to upgrade an image on a device over BLE
communication.

To support over-the-air image upgrade over BLE, a device must be running
a Mynewt application that has newtmgr image management over BLE
transport enabled. For this tutorial, we use the
`bleprph </os/tutorials/bleprph/bleprph-app/>`__ application, which
includes image management over BLE functionality, on an nRF52-DK board.
If you prefer to use a different BLE application, see `Enable Newt
Manager in any app </os/tutorials/add_newtmgr/>`__ to enable newtmgr
image management over BLE transport support in your application.

**Note:** Over-the-air upgrade via newtmgr BLE transport is supported on
Mac OS and Linux. It is not supported on Windows platforms.

Prerequisites
~~~~~~~~~~~~~

Ensure that you meet the following prerequisites:

-  Have Internet connectivity to fetch remote Mynewt components.
-  Have a computer that supports Bluetooth to communicate with the board
   and to build a Mynewt application.
-  Have a Micro-USB cable to connect the board and the computer.
-  Have a Nordic nRF52-DK Development Kit - PCA 10040
-  Install the `Segger JLINK software and documentation
   pack <https://www.segger.com/jlink-software.html>`__.
-  Install the newt tool and toolchains (See `Basic
   Setup </os/get_started/get_started.html>`__).
-  Read the Mynewt OS `Concepts </os/get_started/vocabulary.html>`__
   section.
-  Read the `Bootloader </os/modules/bootloader/bootloader>`__ section
   and understand the Mynewt bootloader concepts.
-  Build and load the **bleprph** application on to an nRF52-DK board
   via a serial connection. See `BLE Peripheral
   App </os/tutorials/bleprph/bleprph-app/>`__.

Reducing the Log Level
~~~~~~~~~~~~~~~


You need to build your application with log level set to INFO or lower.
The default log level for the **bleprph** app is set to DEBUG. The extra
logging causes the communication to timeout. Perform the following to
reduce the log level to INFO, build, and load the application.

.. code-block:: console


    $ newt target amend myperiph syscfg="LOG_LEVEL=1"
    $ newt build myperiph
    $ newt create-image myperiph 1.0.0
    $ newt load myperiph

Upgrading an Image on a Device
~~~~~~~~~~~~~~~

Once you have an application with newtmgr image management with BLE transport support running on a device,
you can use the newtmgr tool to upgrade an image over-the-air.

You must perform the following steps to upgrade an image:

Step 1: Create a newtmgr connection profile to communicate with the
device over BLE. Step 2: Upload the image to the secondary slot (slot 1)
on the device. Step 3: Test the image. Step 4: Confirm and make the
image permanent.

See the `Bootloader </os/modules/bootloader/bootloader>`__ section for
more information on the bootloader, image slots, and boot states.

Step 1: Creating a Newtmgr Connection Profile
~~~~~~~~~~~~~~~

The **bleprph** application sets and advertises ``nimble-bleprph`` as its bluetooth
device address. Run the ``newtmgr conn add`` command to create a newtmgr
connection profile that uses this peer address to communicate with the
device over BLE:

.. code-block:: console


    $ newtmgr conn add mybleprph type=ble connstring="peer_name=nimble-bleprph"
    Connection profile mybleprph successfully added

Verify that the newtmgr tool can communicate with the device and check
the image status on the device:

.. code-block:: console


    $ newtmgr image list -c mybleprph
    Images:
     slot=0
        version: 1.0.0
        bootable: true
        flags: active confirmed
        hash: b8d17c77a03b37603cd9f89fdcfe0ba726f8ddff6eac63011dee2e959cc316c2
    Split status: N/A (0)

The device only has an image loaded on the primary slot (slot 0). It
does not have an image loaded on the secondary slot (slot 1). ### Step
2: Uploading an Image to the Device We create an image with version
2.0.0 for the bleprph application from the ``myperiph`` target and
upload the new image. You can upload a different image.

.. code-block:: console


    $ newt create-image myperiph 2.0.0
    App image succesfully generated: ~/dev/myproj/bin/targets/myperiph/app/apps/bleprph/bleprph.img

Run the ``newtmgr image upload`` command to upload the image:

.. code-block:: console


    $ newtmgr image upload -c mybleprph ~/dev/myproj/bin/targets/myperiph/app/apps/bleprph/bleprph.img
    215
    429
    642
    855
    1068
    1281

    ...

    125953
    126164
    126375
    126586
    126704
    Done

The numbers indicate the number of bytes that the newtmgr tool has
uploaded.

Verify that the image uploaded to the secondary slot on the device
successfully:

.. code-block:: console


    $ newtmgr image list -c mybleprph
    Images:
     slot=0
        version: 1.0.0
        bootable: true
        flags: active confirmed
        hash: b8d17c77a03b37603cd9f89fdcfe0ba726f8ddff6eac63011dee2e959cc316c2
     slot=1
        version: 2.0.0
        bootable: true
        flags:
        hash: 291ebc02a8c345911c96fdf4e7b9015a843697658fd6b5faa0eb257a23e93682
    Split status: N/A (0)

The device now has the uploaded image in the secondary slot (slot 1).
### Step 3: Testing the Image The image is uploaded to the secondary
slot but is not yet active. You must run the ``newtmgr image test``
command to set the image status to **pending** and reboot the device.
When the device reboots, the bootloader copies this image to the primary
slot and runs the image.

.. code-block:: console


    $ newtmgr image test -c mybleprph 291ebc02a8c345911c96fdf4e7b9015a843697658fd6b5faa0eb257a23e93682
    Images:
     slot=0
        version: 1.0.0
        bootable: true
        flags: active confirmed
        hash: b8d17c77a03b37603cd9f89fdcfe0ba726f8ddff6eac63011dee2e959cc316c2
     slot=1
        version: 2.0.0
        bootable: true
        flags: pending
        hash: 291ebc02a8c345911c96fdf4e7b9015a843697658fd6b5faa0eb257a23e93682
    Split status: N/A (0)

The status of the image in the secondary slot is now set to **pending**.

Power the device OFF and ON and run the ``newtmgr image list`` command
to check the image status on the device after the reboot:

.. code-block:: console


    $ newtmgr image list -c mybleprph
    Images:
     slot=0
        version: 2.0.0
        bootable: true
        flags: active
        hash: 291ebc02a8c345911c96fdf4e7b9015a843697658fd6b5faa0eb257a23e93682
     slot=1
        version: 1.0.0
        bootable: true
        flags: confirmed
        hash: b8d17c77a03b37603cd9f89fdcfe0ba726f8ddff6eac63011dee2e959cc316c2
    Split status: N/A (0)

The uploaded image is now active and running in the primary slot. The
image, however, is not confirmed. The confirmed image is in the
secondary slot. On the next reboot, the bootloader reverts to using the
confirmed image. It copies the confirmed image to the primary slot and
runs the image when the device reboots. You need to confirm and make the
uploaded image in the primary slot permanent. ### Step 4: Confirming the
Image Run the ``newtmgr image confirm`` command to confirm and make the
uploaded image permanent. Since the uploaded image is currently the
active image, you can confirm the image setup without specifying the
image hash value in the command:

.. code-block:: console


    $ newtmgr image confirm -c mybleprph
    Images:
     slot=0
        version: 2.0.0
        bootable: true
        flags: active confirmed
        hash: 291ebc02a8c345911c96fdf4e7b9015a843697658fd6b5faa0eb257a23e93682
     slot=1
        version: 1.0.0
        bootable: true
        flags:
        hash: b8d17c77a03b37603cd9f89fdcfe0ba726f8ddff6eac63011dee2e959cc316c2
    Split status: N/A (0)

The uploaded image is now the active and confirmed image. You have
successfully upgraded an image over-the-air.
