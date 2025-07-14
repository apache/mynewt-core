### Nanopb SPI Simple Example

##### Overview
********

This application is based on Nanopb example called Simple.
It contains source and header file generated with Nanopb.
The header file defines one structure (message) with one int field
called lucky_number. The application shows how to use basic Nanopb API and
configures the SPI to send the message and receive it on the other device.
Two devices are required for this to work:
1. MASTER - encodes and sends one message per second over the SPI.
   After sending one message the lucky_number is incremented, encoded
   and sent again. Toggles the LED after each sent message.
2. SLAVE - after receiving the message it decodes it and prints
   the lucky_number value. Toggles the LED after each received
   message.
