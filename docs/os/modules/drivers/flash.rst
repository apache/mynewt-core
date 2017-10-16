flash
-----

The flash driver subsystem is a work in progress which aims at
supporting common external SPI/I2C flash/eeprom memory chips. This is
equivalent to what Linux calls ``MTD`` for
``Memory Technology Devices``.

At the moment the only ``flash`` device that is already supported is the
AT45DBxxx SPI flash family with the ``at45db`` driver.

The flash driver aims for full compatibility with the ``hal_flash`` API,
which means initialization and usage can be performed by any ``fs`` that
supports the ``hal_flash`` interface.

Initialization
^^^^^^^^^^^^^^

To be compatible with the standard ``hal_flash`` interface, the
``at45db`` driver embeds a ``struct hal_flash`` to its own
``struct at45db_dev``. The whole ``at45db_dev`` struct is shown below.

.. code:: c

    struct at45db_dev {
        struct hal_flash hal;
        struct hal_spi_settings *settings;
        int spi_num;
        void *spi_cfg;                  /** Low-level MCU SPI config */
        int ss_pin;
        uint32_t baudrate;
        uint16_t page_size;             /** Page size to be used, valid: 512 and 528 */
        uint8_t disable_auto_erase;     /** Reads and writes auto-erase by default */
    };

To ease with initialization a helper function ``at45db_default_config``
was added. It returns an already initialized ``struct at45db_dev``
leaving the user with just having to provide the SPI related config.

To initialize the device, pass the ``at45db_dev`` struct to
``at45db_init``.

.. code:: c

    int at45db_init(const struct hal_flash *dev);

For low-level access to the device the following functions are provided:

.. code:: c

    int at45db_read(const struct hal_flash *dev, uint32_t addr, void *buf,
                    uint32_t len);
    int at45db_write(const struct hal_flash *dev, uint32_t addr, const void *buf,
                     uint32_t len);
    int at45db_erase_sector(const struct hal_flash *dev, uint32_t sector_address);
    int at45db_sector_info(const struct hal_flash *dev, int idx, uint32_t *address,
                           uint32_t *sz);

Also, ``nffs`` is able to run on the device due to the fact that
standard ``hal_flash`` interface compatibility is provided. Due to
current limitations of ``nffs``, it can only run on ``at45db`` if the
internal flash of the MCU is not being used.

Dependencies
^^^^^^^^^^^^

To include the ``at45db`` driver on a project, just include it as a
dependency in your pkg.yml:

::

    pkg.deps:
        - hw/drivers/flash/at45db

Header file
^^^^^^^^^^^

The ``at45db`` SPI flash follows the standard ``hal_flash`` interface
but requires that a special struct

.. code:: c

    #include <at45db/at45db.h>

Example
^^^^^^^

This following examples assume that the ``at45db`` is being used on a
STM32F4 MCU.

.. code:: c

    static const int SPI_SS_PIN   = MCU_GPIO_PORTA(4);
    static const int SPI_SCK_PIN  = MCU_GPIO_PORTA(5);
    static const int SPI_MISO_PIN = MCU_GPIO_PORTA(6);
    static const int SPI_MOSI_PIN = MCU_GPIO_PORTA(7);

    struct stm32f4_hal_spi_cfg spi_cfg = {
        .ss_pin   = SPI_SS_PIN,
        .sck_pin  = SPI_SCK_PIN,
        .miso_pin = SPI_MISO_PIN,
        .mosi_pin = SPI_MOSI_PIN,
        .irq_prio = 2
    };

    struct at45db_dev *my_at45db_dev = NULL;

    my_at45db_dev = at45db_default_config();
    my_at45db_dev->spi_num = 0;
    my_at45db_dev->spi_cfg = &spi_cfg;
    my_at45db_dev->ss_pin = spi_cfg.ss_pin;

    rc = at45db_init((struct hal_flash *) my_at45db_dev);
    if (rc) {
        /* XXX: error handling */
    }

The enable ``nffs`` to run on the ``at45db``, the ``flash_id`` 0 needs
to map to provide a mapping from 0 to this struct.

.. code:: c

    const struct hal_flash *
    hal_bsp_flash_dev(uint8_t id)
    {
        if (id != 0) {
            return NULL;
        }
        return &my_at45db_dev;
    }
