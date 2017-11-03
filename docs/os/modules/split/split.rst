Split Images
============

Description
-----------

The split image mechanism divides a target into two separate images: one
capable of image upgrade; the other containing application code. By
isolating upgrade functionality to a separate image, the application can
support over-the-air upgrade without dedicating flash space to network
stack and management code.

Concept
-------

Mynewt supports three image setups:

+-----------+--------------------------------------------+
| Setup     | Description                                |
+===========+============================================+
| Single    | One large image; upgrade not supported.    |
+-----------+--------------------------------------------+
| Unified   | Two standalone images.                     |
+-----------+--------------------------------------------+
| Split     | Kernel in slot 0; application in slot 1.   |
+-----------+--------------------------------------------+

Each setup has its tradeoffs. The Single setup gives you the most flash
space, but doesn't allow you to upgrade after manufacturing. The Unified
setup allows for a complete failover in case a bad image gets uploaded,
but requires a lot of redundancy in each image, limiting the amount of
flash available to the application. The Split setup sits somewhere
between these two options.

Before exploring the split setup in more detail, it might be helpful to
get a basic understanding of the Mynewt boot sequence. The boot process
is summarized below.

Boot Sequence - Single
^^^^^^^^^^^^^^^^^^^^^^

In the Single setup, there is no boot loader. Instead, the image is
placed at address 0. The hardware boots directly into the image code.
Upgrade is not possible because there is no boot loader to move an
alternate image into place.

Boot Sequence - Unified
^^^^^^^^^^^^^^^^^^^^^^^

In the Unified setup, the boot loader is placed at address 0. At
startup, the boot loader arranges for the correct image to be in image
slot 0, which may entail swapping the contents of the two image slots.
Finally, the boot loader jumps to the image in slot 0.

Boot Sequence - Split
^^^^^^^^^^^^^^^^^^^^^

The Split setup differs from the other setups mainly in that a target is
not fully contained in a single image. Rather, the target is partitioned
among two separate images: the *loader*, and the *application*.
Functionality is divided among these two images as follows:

1. Loader:

   -  Mynewt OS.
   -  Network stack for connectivity during upgrade e.g. BLE stack.
   -  Anything else required for image upgrade.

2. Application:

   -  Parts of Mynewt not required for image upgrade.
   -  Application-specific code.

The loader image serves three purposes:

1. *Second-stage boot loader:* it jumps into the application image at
   start up.
2. *Image upgrade server:* the user can upgrade to a new loader +
   application combo, even if an application image is not currently
   running.
3. *Functionality container:* the application image can directly access
   all the code present in the loader image

From the perspective of the boot loader, a loader image is identical to
a plain unified image. What makes a loader image different is a change
to its start up sequence: rather than starting the Mynewt OS, it jumps
to the application image in slot 1 if one is present.

Tutorial
--------

Building a Split Image
~~~~~~~~~~~~~~~~~~~~~~

We will be referring to the nRF51dk for examples in this document. Let's
take a look at this board's flash map (defined in
``hw/bsp/nrf51dk/bsp.yml``):

+---------------------+--------------+-------------+
| Name                | Offset       | Size (kB)   |
+=====================+==============+=============+
| Boot loader         | 0x00000000   | 16          |
+---------------------+--------------+-------------+
| Reboot log          | 0x00004000   | 16          |
+---------------------+--------------+-------------+
| Image slot 0        | 0x00008000   | 110         |
+---------------------+--------------+-------------+
| Image slot 1        | 0x00023800   | 110         |
+---------------------+--------------+-------------+
| Image scratch       | 0x0003f000   | 2           |
+---------------------+--------------+-------------+
| Flash file system   | 0x0003f800   | 2           |
+---------------------+--------------+-------------+

The application we will be building is
`bleprph <../../tutorials/bleprph>`__. First, we create a target to tie
our BSP and application together.

::

    newt target create bleprph-nrf51dk
    newt target set bleprph-nrf51dk                     \
        app=@apache-mynewt-core/apps/bleprph            \
        bsp=@apache-mynewt-core/hw/bsp/nrf51dk          \
        build_profile=optimized                         \
        syscfg=BLE_LL_CFG_FEAT_LE_ENCRYPTION=0:BLE_SM_LEGACY=0

The two syscfg settings disable bluetooth security and keep the code
size down.

We can verify the target using the ``target show`` command:

::

    [~/tmp/myproj2]$ newt target show bleprph-nrf51dk
    targets/bleprph-nrf51dk
        app=@apache-mynewt-core/apps/bleprph
        bsp=@apache-mynewt-core/hw/bsp/nrf51dk
        build_profile=optimized
        syscfg=BLE_LL_CFG_FEAT_LE_ENCRYPTION=0:BLE_SM_LEGACY=0

Next, build the target:

::

    [~/tmp/myproj2]$ newt build bleprph-nrf51dk
    Building target targets/bleprph-nrf51dk
    # [...]
    Target successfully built: targets/bleprph-nrf51dk

With our target built, we can view a code size breakdown using the
``newt size <target>`` command. In the interest of brevity, the smaller
entries are excluded from the below output:

::

    [~/tmp/myproj2]$ newt size bleprph-nrf51dk
    Size of Application Image: app
      FLASH     RAM
       2446    1533 apps_bleprph.a
       1430     104 boot_bootutil.a
       1232       0 crypto_mbedtls.a
       1107       0 encoding_cborattr.a
       2390       0 encoding_tinycbor.a
       1764       0 fs_fcb.a
       2959     697 hw_drivers_nimble_nrf51.a
       4126     108 hw_mcu_nordic_nrf51xxx.a
       8161    4049 kernel_os.a
       2254      38 libc_baselibc.a
       2612       0 libgcc.a
       2232      24 mgmt_imgmgr.a
       1499      44 mgmt_newtmgr_nmgr_os.a
      23918    1930 net_nimble_controller.a
      28537    2779 net_nimble_host.a
       2207     205 sys_config.a
       1074     197 sys_console_full.a
       3268      97 sys_log.a
       1296       0 time_datetime.a

    objsize
       text    data     bss     dec     hex filename
     105592    1176   13392  120160   1d560 /home/me/tmp/myproj2/bin/targets/bleprph-nrf51dk/app/apps/bleprph/bleprph.elf

The full image text size is about 103kB (where 1kB = 1024 bytes). With
an image slot size of 110kB, this leaves only about 7kB of flash for
additional application code and data. Not good. This is the situation we
would be facing if we were using the Unified setup.

The Split setup can go a long way in solving our problem. Our unified
bleprph image consists mostly of components that get used during an
image upgrade. By using the Split setup, we turn the unified image into
two separate images: the loader and the application. The functionality
related to image upgrade can be delegated to the loader image, freeing
up a significant amount of flash in the application image slot.

Let's create a new target to use with the Split setup. We designate a
target as a split target by setting the ``loader`` variable. In our
example, we are going to use ``bleprph`` as the loader, and ``splitty``
as the application. ``bleprph`` makes sense as a loader because it
contains the BLE stack and everything else required for an image
upgrade.

::

    newt target create split-nrf51dk
    newt target set split-nrf51dk                       \
        loader=@apache-mynewt-core/apps/bleprph         \
        app=@apache-mynewt-core/apps/splitty            \
        bsp=@apache-mynewt-core/hw/bsp/nrf51dk          \
        build_profile=optimized                         \
        syscfg=BLE_LL_CFG_FEAT_LE_ENCRYPTION=0:BLE_SM_LEGACY=0

Verify that the target looks correct:

::

    [~/tmp/myproj2]$ newt target show split-nrf51dk
    targets/split-nrf51dk
        app=@apache-mynewt-core/apps/splitty
        bsp=@apache-mynewt-core/hw/bsp/nrf51dk
        build_profile=optimized
        loader=@apache-mynewt-core/apps/bleprph
        syscfg=BLE_LL_CFG_FEAT_LE_ENCRYPTION=0:BLE_SM_LEGACY=0

Now, let's build the new target:

::

    [~/tmp/myproj2]$ newt build split-nrf51dk
    Building target targets/split-nrf51dk
    # [...]
    Target successfully built: targets/split-nrf51dk

And look at the size breakdown (again, smaller entries are removed):

::

    [~/tmp/myproj2]$ newt size split-nrf51dk
    Size of Application Image: app
      FLASH     RAM
       3064     251 sys_shell.a

    objsize
       text    data     bss     dec     hex filename
       4680     112   17572   22364    575c /home/me/tmp/myproj2/bin/targets/split-nrf51dk/app/apps/splitty/splitty.elf

    Size of Loader Image: loader
      FLASH     RAM
       2446    1533 apps_bleprph.a
       1430     104 boot_bootutil.a
       1232       0 crypto_mbedtls.a
       1107       0 encoding_cborattr.a
       2390       0 encoding_tinycbor.a
       1764       0 fs_fcb.a
       3168     705 hw_drivers_nimble_nrf51.a
       4318     109 hw_mcu_nordic_nrf51xxx.a
       8285    4049 kernel_os.a
       2274      38 libc_baselibc.a
       2612       0 libgcc.a
       2232      24 mgmt_imgmgr.a
       1491      44 mgmt_newtmgr_nmgr_os.a
      25169    1946 net_nimble_controller.a
      31397    2827 net_nimble_host.a
       2259     205 sys_config.a
       1318     202 sys_console_full.a
       3424      97 sys_log.a
       1053      60 sys_stats.a
       1296       0 time_datetime.a

    objsize
       text    data     bss     dec     hex filename
     112020    1180   13460  126660   1eec4 /home/me/tmp/myproj2/bin/targets/split-nrf51dk/loader/apps/bleprph/bleprph.elf

The size command shows two sets of output: one for the application, and
another for the loader. The addition of the split functionality did make
bleprph slightly bigger, but notice how small the application is: 4.5
kB! Where before we only had 7 kB left, now we have 105.5 kB.
Furthermore, all the functionality in the loader is available to the
application at any time. For example, if your application needs
bluetooth functionality, it can use the BLE stack present in the loader
instead of containing its own copy.

Finally, let's deploy the split image to our nRF51dk board. The
procedure here is the same as if we were using the Unified setup, i.e.,
via either the ``newt load`` or ``newt run`` command.

::

    [~/repos/mynewt/core]$ newt load split-nrf51dk 0
    Loading app image into slot 2
    Loading loader image into slot 1

Image Management
~~~~~~~~~~~~~~~~

Retrieve Current State (image list)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Image management in the split setup is a bit more complicated than in
the unified setup. You can determine a device's image management state
with the ``newtmgr image list`` command. Here is how a device responds
to this command after our loader + application combo has been deployed:

::

    [~/tmp/myproj2]$ newtmgr -c A600ANJ1 image list
    Images:
     slot=0
        version: 0.0.0
        bootable: true
        flags: active confirmed
        hash: 948f118966f7989628f8f3be28840fd23a200fc219bb72acdfe9096f06c4b39b
     slot=1
        version: 0.0.0
        bootable: false
        flags:
        hash: 78e4d263eeb5af5635705b7cae026cc184f14aa6c6c59c6e80616035cd2efc8f
    Split status: matching

There are several interesting things about this response:

1. *Two images:* This is expected; we deployed both a loader image and
   an application image.
2. *bootable flag:* Notice slot 0's bootable flag is set, while slot 1's
   is not. This tells us that slot 0 contains a loader and slot 1
   contains an application. If an image is bootable, it can be booted
   directly from the boot loader. Non-bootable images can only be
   started from a loader image.
3. *flags:* Slot 0 is ``active`` and ``confirmed``; none of slot 1's
   flags are set. The ``active`` flag indicates that the image is
   currently running; the ``confirmed`` flag indicates that the image
   will continue to be used on subsequent reboots. Slot 1's lack of
   enabled flags indicates that the image is not being used at all.
4. *Split status:* The split status field tells you if the loader and
   application are compatible. A loader + application combo is
   compatible only if both images were built at the same time with
   ``newt``. If the loader and application are not compatible, the
   loader will not boot into the application.

Enabling a Split Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

By default, the application image in slot 1 is disabled. This is
indicated in the ``image list`` response above. When you deploy a loader
/ application combo to your device, the application image won't actually
run. Instead, the loader will act as though an application image is not
present and remain in "loader mode". Typically, a device in loader mode
simply acts as an image management server, listening for an image
upgrade or a request to activate the application image.

Use the following command sequence to enable the split application
image:

1. Tell device to "test out" the application image on next boot
   (``newtmgr image test <application-image-hash>``).
2. Reboot device (``newtmgr reset``).
3. Make above change permanent (``newtmgr image confirm``).

After the above sequence, a ``newtmgr image list`` command elicits the
following response:

::

    [~/tmp/myproj2]$ newtmgr -c A600ANJ1 image confirm
    Images:
     slot=0
        version: 0.0.0
        bootable: true
        flags: active confirmed
        hash: 948f118966f7989628f8f3be28840fd23a200fc219bb72acdfe9096f06c4b39b
     slot=1
        version: 0.0.0
        bootable: false
        flags: active confirmed
        hash: 78e4d263eeb5af5635705b7cae026cc184f14aa6c6c59c6e80616035cd2efc8f
    Split status: matching

The ``active confirmed`` flags value on both slots indicates that both
images are permanently running.

Image Upgrade
~~~~~~~~~~~~~

First, let's review of the image upgrade process for the Unified setup.
The user upgrades to a new image in this setup with the following steps:

Image Upgrade - Unified
^^^^^^^^^^^^^^^^^^^^^^^

1. Upload new image to slot 1 (``newtmgr image upload <filename>``).
2. Tell device to "test out" the new image on next boot
   (``newtmgr image test <image-hash>``).
3. Reboot device (``newtmgr reset``).
4. Make new image permanent (``newtmgr image confirm``).

Image Upgrade - Split
^^^^^^^^^^^^^^^^^^^^^

The image upgrade process is a bit more complicated in the Split setup.
It is more complicated because two images need to be upgraded (loader
and application) rather than just one. The split upgrade process is
described below:

1.  Disable split functionality; we need to deactivate the application
    image in slot 1 (``newtmgr image test <current-loader-hash>``).
2.  Reboot device (``newtmgr reset``).
3.  Make above change permanent (``newtmgr image confirm``).
4.  Upload new loader to slot 1 (``newtmgr image upload <filename>``).
5.  Tell device to "test out" the new loader on next boot
    (``newtmgr image test <new-loader-hash>``).
6.  Reboot device (``newtmgr reset``).
7.  Make above change of loader permanent (``newtmgr image confirm``).
8.  Upload new application to slot 1
    (``newtmgr image upload <filename>``).
9.  Tell device to "test out" the new application on next boot
    (``newtmgr image test <new-application-hash>``).
10. Reboot device (``newtmgr reset``).
11. Make above change of application permanent
    (``newtmgr image confirm``).

When performing this process manually, it may be helpful to use
``image list`` to check the image management state as you go.

Syscfg
------

Syscfg is Mynewt's system-wide configuration mechanism. In a split
setup, there is a single umbrella syscfg configuration that applies to
both the loader and the application. Consequently, overriding a value in
an application-only package potentially affects the loader (and
vice-versa).

Loaders
-------

The following applications have been enabled as loaders. You may choose
to build your own loader application, and these can serve as samples.

-  @apache-mynewt-core/apps/slinky
-  @apache-mynewt-core/apps/bleprph

Split Apps
----------

The following applications have been enabled as split applications. If
you choose to build your own split application these can serve as
samples. Note that slinky can be either a loader image or an application
image.

-  @apache-mynewt-core/apps/slinky
-  @apache-mynewt-core/apps/splitty

Theory of Operation
-------------------

A split image is built as follows:

First newt builds the application and loader images separately to ensure
they are consistent (no errors) and to generate elf files which can
inform newt of the symbols used by each part.

Then newt collects the symbols used by both application and loader in
two ways. It collects the set of symbols from the ``.elf`` files. It
also collects all the possible symbols from the ``.a`` files for each
application.

Newt builds the set of packages that the two applications share. It
ensures that all the symbols used in those packages are matching. NOTE:
because of features and #ifdefs, its possible for the two package to
have symbols that are not the same. In this case newt generates an error
and will not build a split image.

Then newt creates the list of symbols that the two applications share
from those packages (using the .elf files).

Newt re-links the loader to ensure all of these symbols are present in
the loader application (by forcing the linker to include them in the
``.elf``).

Newt builds a special copy of the loader.elf with only these symbols
(and the handful of symbols discussed in the linking section above).

Finally, newt links the application, replacing the common .a libraries
with the special loader.elf image during the link.
