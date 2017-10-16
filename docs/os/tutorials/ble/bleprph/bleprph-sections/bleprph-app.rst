BLE Peripheral App
==================

Overview
~~~~~~~~

Now that we've gone through how BLE Apps are contructed, how they
function, and how all the parts fit together let's try out a BLE
Peripheral App to see how it all works.

Prerequisites
~~~~~~~~~~~~~

-  You should have a BLE Central App of some sort to connect with. On
   Mac OS or iOS, you can use
   `LightBlue <https://itunes.apple.com/us/app/lightblue-explorer-bluetooth/id557428110?mt=8>`__
   which is a free app to browse and connect to BLE Peripheral devices.

Create a New Target
~~~~~~~~~~~~~~~~~~~

You can create a new project instead, but this tutorial will simply use
the previously created bletiny project and add a new target for the BLE
Peripheral

.. code-block:: console

    $ newt target create myperiph
    Target targets/myperiph successfully created
    $ newt target set myperiph bsp=@apache-mynewt-core/hw/bsp/nrf52dk
    Target targets/myperiph successfully set target.bsp to @apache-mynewt-core/hw/bsp/nrf52dk
    $ newt target set myperiph app=@apache-mynewt-core/apps/bleprph
    Target targets/myperiph successfully set target.app to @apache-mynewt-core/apps/bleprph
    $ newt target set myperiph build_profile=optimized
    Target targets/myperiph successfully set target.build_profile to optimized
    $ newt build myperiph
    Building target targets/myperiph
    ...
    Linking ~/dev/nrf52dk/bin/targets/myperiph/app/apps/bleprph/bleprph.elf
    Target successfully built: targets/myperiph
    $ newt create-image myperiph 1.0.0
    App image succesfully generated: ~/dev/nrf52dk/bin/targets/myperiph/app/apps/bleprph/bleprph.img
    $ newt load myperiph
    Loading app image into slot 1

Now if you reset the board, and fire up your BLE Central App, you should
see a new peripheral device called 'nimble-bleprph'.

.. figure:: ../pics/LightBlue-1.jpg
   :alt: LightBlue iOS App with nimble-bleprph device

   LightBlue

Now that you can see the device, you can begin to interact with the
advertised service.

Click on the device and you'll establish a connection.

.. figure:: ../pics/LightBlue-2.jpg
   :alt: LightBlue iOS App connected to the nimble-bleprph device

   LightBlue

Now that you're connected, you can see the Services that are being
advertised.

Scroll to the bottom and you will see a Read Characteristic, and a
Read/Write Characteristic.

.. figure:: ../pics/LightBlue-3.jpg
   :alt: LightBlue iOS App connected to the nimble-bleprph device

   LightBlue

Just click on the Read Write Characteristic and you will see the
existing value.

.. figure:: ../pics/LightBlue-4.jpg
   :alt: LightBlue iOS App with nimble-bleprph device Characteristic

   LightBlue

Type in a new value.

.. figure:: ../pics/LightBlue-5.jpg
   :alt: LightBlue iOS App Value Change

   LightBlue

And you will see the new value reflected.

.. figure:: ../pics/LightBlue-6.jpg
   :alt: LightBlue iOS App with nimble-bleprph new value

   LightBlue

If you still have your console connected, you will be able to see the
connection requests, and pairing, happen on the device as well.

.. code:: hl_lines="1"

    258894:[ts=2022609336ssb, mod=64 level=1] connection established; status=0 handle=1 our_ota_addr_type=0 our_ota_addr=0a:0a:0a:0a:0a:0a our_id_addr_type=0 our_id_addr=0a:0a:0a:0a:0a:0a peer_ota_addr_type=1 peer_ota_addr=7f:be:d4:44:c0:d4 peer_id_addr_type=1 peer_id_addr=7f:be:d4:44:c0:d4 conn_itvl=24 conn_latency=0 supervision_timeout=72 encrypted=0 authenticated=0 bonded=0
    258904:[ts=2022687456ssb, mod=64 level=1]
    258917:[ts=2022789012ssb, mod=64 level=1] mtu update event; conn_handle=1 cid=4 mtu=185
    258925:[ts=2022851508ssb, mod=64 level=1] subscribe event; conn_handle=1 attr_handle=14 reason=1 prevn=0 curn=0 previ=0 curi=1
    261486:[ts=2042859320ssb, mod=64 level=1] encryption change event; status=0 handle=1 our_ota_addr_type=0 our_ota_addr=0a:0a:0a:0a:0a:0a our_id_addr_type=0 our_id_addr=0a:0a:0a:0a:0a:0a peer_ota_addr_type=1 peer_ota_addr=7f:be:d4:44:c0:d4 peer_id_addr_type=1 peer_id_addr=7f:be:d4:44:c0:d4 conn_itvl=24 conn_latency=0 supervision_timeout=72 encrypted=1 authenticated=0 bonded=1
    261496:[ts=2042937440ssb, mod=64 level=1]

Congratulations! You've just built and connected your first BLE
Peripheral device!
