mmc
---

The MMC driver provides support for SPI based MMC/SDcard interfaces. It
exports a ``disk_ops`` struct that can be used by any FS. Currently only
``fatfs`` can run over MMC.

Initialization
^^^^^^^^^^^^^^

.. code:: c

    int mmc_init(int spi_num, void *spi_cfg, int ss_pin)

Initializes the mmc driver to be used by a FS.

MMC uses the ``hal_gpio`` interface to access the SPI ``ss_pin`` and the
``hal_spi`` interface for the communication with the card. ``spi_cfg``
must be a hw dependent structure used by ``hal_spi_init`` to initialize
the SPI subsystem.

Dependencies
^^^^^^^^^^^^

To include the ``mmc`` driver on a project, just include it as a
dependency in your pkg.yml:

::

    pkg.deps:
        - hw/drivers/mmc

Returned values
^^^^^^^^^^^^^^^

MMC functions return one of the following status codes:

+-------------------------+--------------------------------------------------------+
| Return code             | Description                                            |
+=========================+========================================================+
| MMC\_OK                 | Success.                                               |
+-------------------------+--------------------------------------------------------+
| MMC\_CARD\_ERROR        | General failure on the card.                           |
+-------------------------+--------------------------------------------------------+
| MMC\_READ\_ERROR        | Error reading from the card.                           |
+-------------------------+--------------------------------------------------------+
| MMC\_WRITE\_ERROR       | Error writing to the card.                             |
+-------------------------+--------------------------------------------------------+
| MMC\_TIMEOUT            | Timed out waiting for the execution of a command.      |
+-------------------------+--------------------------------------------------------+
| MMC\_PARAM\_ERROR       | An invalid parameter was given to a function.          |
+-------------------------+--------------------------------------------------------+
| MMC\_CRC\_ERROR         | CRC error reading card.                                |
+-------------------------+--------------------------------------------------------+
| MMC\_DEVICE\_ERROR      | Tried to use an invalid device.                        |
+-------------------------+--------------------------------------------------------+
| MMC\_RESPONSE\_ERROR    | A command received an invalid response.                |
+-------------------------+--------------------------------------------------------+
| MMC\_VOLTAGE\_ERROR     | The interface doesn't support the requested voltage.   |
+-------------------------+--------------------------------------------------------+
| MMC\_INVALID\_COMMAND   | The interface haven't accepted some command.           |
+-------------------------+--------------------------------------------------------+
| MMC\_ERASE\_ERROR       | Error erasing the current card.                        |
+-------------------------+--------------------------------------------------------+
| MMC\_ADDR\_ERROR        | Tried to access an invalid address.                    |
+-------------------------+--------------------------------------------------------+

Header file
^^^^^^^^^^^

.. code:: c

    #include "mmc/mmc.h"

Example
^^^^^^^

This example runs on the STM32F4-Discovery and prints out a listing of
the root directory on the currently installed card.

.. code:: c

    // NOTE: error handling removed for clarity!

    struct stm32f4_hal_spi_cfg spi_cfg = {
        .ss_pin   = SPI_SS_PIN,
        .sck_pin  = SPI_SCK_PIN,
        .miso_pin = SPI_MISO_PIN,
        .mosi_pin = SPI_MOSI_PIN,
        .irq_prio = 2
    };

    mmc_init(0, &spi_cfg, spi_cfg.ss_pin);
    disk_register("mmc0", "fatfs", &mmc_ops);

    fs_opendir("mmc0:/", &dir);

    while (1) {
        rc = fs_readdir(dir, &dirent);
        if (rc == FS_ENOENT) {
            break;
        }

        fs_dirent_name(dirent, sizeof(out_name), out_name, &u8_len);
        printf("%s\n", out_name);
    }

    fs_closedir(dir);
