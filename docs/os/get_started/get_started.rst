Basic Setup
===========

.. toctree::
   :hidden:

   native_install_intro
   docker
   project_create
   serial_access

If you are curious about Mynewt and want to get a quick feel for the
project, you've come to the right place. We have two options for you:

**Option 1 (Recommended)** allows you to install the Newt tool,
instances of the Mynewt OS (for simulated targets), and toolchains for
developing embedded software (e.g. GNU toolchain) natively on your
laptop or computer. We have tried to make the process easy. For example,
for the Mac OS we created brew formulas.

We recommend this option if you are familiar with such environments or
are concerned about performance on your machine. Follow the instructions
in the `Native Install Option <native_install_option>`__ if you prefer
this option.

**Option 2** is an easy, self-contained way to get up and running with
Mynewt - but has limitations! The Newt tool and build toolchains are all
available in a single `All-in-one Docker Container <docker_container_option.html>`__ that
you can install on your laptop or computer.

However, this is not a long-term option since support is not likely for
all features useful or critical to embedded systems development. For
example, USB device mapping available in the Docker toolkit is no longer
available in the new Docker releases. The Docker option is also
typically slower than the native install option.

You can then proceed with the instructions on how to `Create Your First
Project <project_create.html>`__ - on simulated hardware.

Upon successful start, several tutorials await your eager attention!

**Send us an email on the dev@ mailing list if you have comments or
suggestions!** If you haven't joined the mailing list, you will find the
links `here <../../community.html>`__.
