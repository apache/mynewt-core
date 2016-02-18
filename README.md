# Apache Mynewt Repository 

# Overview

This is the main Apache Mynewt development repository.  It contains the major
packages that make up the Apache Mynewt Operating System.  These packages are
contained in the following directories: 

- libs: Contains the core of the RTOS (libs/os) and a number of helper 
libraries for building applications.  Including a console (libs/console), 
shell (libs/shell), and newtmgr, which supports software upgrade and 
remote fetching of logs and statistics (libs/newtmgr).  

- net: Contains the networking packages.  Right now, net/ just contains
the nimble package.  Nimble is a full Bluetooth host and controller 
implmentation, that is written from the ground up for the Apache Mynewt
Operating System.   

- hw: Contains the HW specific support packages.  Board Support Packages 
are located in hw/bsp, and the MCU specific definitions they rely on 
are located in hw/mcu.  Finally, there is a HAL (Hardware Abstraction 
Layer) stored in hw/hal, even though the implementation of various 
HALs are stored in the MCU specific definitions.  

- fs: Contains the FS package (fs/fs), which is the high-level Apache Mynewt 
file system API.   A specific implementation of that FS, is NFFS (Newtron
File System.)  The Newtron file system is a FS that has been built from 
the ground-up in Apache Mynewt, designed to be optimized for small 
(64KB-32MB) flashes.

In addition to the package directories, there is a special type of package
called a project, which defines an embedded build project (it has a 
main() function.)  In the Apache Mynewt development repository there are a 
set of standard projects, which can be used as examples to fasten your
own project:

* boot: Project to build the bootloader for test platforms. 
* blinky: The minimal packages to build the OS, and blink a
LED!  
* slinky: A slightly more complex project that includes the 
console and shell libraries.  
* test: Test project which can be compiled either with the simulator, or 
  on a per-architecture basis.  Test will run all the package's unit 
  tests. 

# License 

The code in this repository is all under either the Apache 2 license, or a 
license compatible with the Apache 2 license.  See the LICENSE file for more 
information. 

# Export restrictions

This distribution includes cryptographic software. The country in which you 
currently reside may have restrictions on the import, possession, use, and/or 
re-export to another country, of encryption software. BEFORE using any encryption
software, please check your country's laws, regulations and policies concerning
the import, possession, or use, and re-export of encryption software, to see if
this is permitted. See <http://www.wassenaar.org/> for more information.

The U.S. Government Department of Commerce, Bureau of Industry and Security (BIS), 
has classified this software as Export Commodity Control Number (ECCN) 5D002.C.1, 
which includes information security software using or performing cryptographic 
functions with asymmetric algorithms. The form and manner of this Apache Software 
Foundation distribution makes it eligible for export under the License Exception ENC 
Technology Software Unrestricted (TSU) exception (see the BIS Export Administration 
Regulations, Section 740.13) for both object code and source code.

The following provides more details on the included cryptographic software: 
https://tls.mbed.org/supported-ssl-ciphersuites.

# Contact 

For any questions on the larva repository, please contact the Apache Mynewt developer's 
list (dev@mynewt.incubator.apache.org).  
