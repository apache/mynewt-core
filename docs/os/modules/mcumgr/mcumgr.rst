MCU Manager (mcumgr)
--------------------

.. toctree::
   :maxdepth: 1

mcumgr, a derivative of newtmgr, is a device/image/embedded OS  management library with pluggable transport and encoding components and is, by design, operating system and hardware independent. It relies on hardware porting layers from the operating system it runs on. Currently, mcumgr runs on both the Apache Mynewt and Zephyr operating systems.

So how is it different from newtmgr? There is one substantial difference between the two: newtmgr supports two wire formats - NMP (plain newtmgr protocol) and OMP (CoAP newtmgr protocol).  mcumgr only supports NMP (called "SMP" in mcumgr).

NMP is a simple binary format: 8-byte header plus CBOR payload
OMP uses CoAP requests to the `/omgr` CoAP resource

A request has the same effect on the receiving device regardless of wire format, but OMP fits more cleanly in a system that is already using CoAP. Documentation on mcumgr can be found in the source code repository: `mynewt-mcumgr <https://github.com/apache/mynewt-mcumgr>`_

There has been some discussion about combining all this functionality into a single library (mcumgr). Your views are welcome on dev@mynewt.apache.org. 
