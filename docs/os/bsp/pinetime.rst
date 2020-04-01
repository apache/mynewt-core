..
  #
  # Copyright 2020 Casper Meijn <casper@meijn.net>
  #
  # Licensed under the Apache License, Version 2.0 (the "License");
  # you may not use this file except in compliance with the License.
  # You may obtain a copy of the License at
  #
  #     http://www.apache.org/licenses/LICENSE-2.0
  #
  # Unless required by applicable law or agreed to in writing, software
  # distributed under the License is distributed on an "AS IS" BASIS,
  # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  # See the License for the specific language governing permissions and
  # limitations under the License.
  #

PineTime smartwatch
===================

This page is about the board support package for the Pine64 PineTime smartwatch. 
You can find some general documentation at the `device wiki <https://wiki.pine64.org/index.php/PineTime>`__. 
You could buy a dev kit in the `store <https://store.pine64.org/?product=pinetime-dev-kit>`__.

.. contents::
  :local:
  :depth: 2

Status
~~~~~~

Currently the status is: incomplete.

The board support package contains the code for booting the device and the pin 
definitions. This means you can load an application like blinky, but no 
pheriphirals can be used. New drivers will be added in the future.

Tutorials
~~~~~~~~~

-  :doc:`../../tutorials/blinky/pinetime`
