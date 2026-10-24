hal\_flash\_int
===============

The API that flash drivers have to implement.

Description
~~~~~~~~~~~

The BSP for the hardware will implement the structs defined in this API.

Definition
~~~~~~~~~~

`hal\_flash\_int.h <https://github.com/apache/mynewt-core/blob/master/hw/hal/include/hal/hal_flash_int.h>`__

Registering flash devices
~~~~~~~~~~~~~~~~~~~~~~~~~

Flash drivers register their ``struct hal_flash`` with ``HAL_FLASH_DEV()``.
With ``HAL_FLASH_DEV_TABLE`` set (default), ``hw/hal`` provides
``hal_bsp_flash_dev()`` that returns the device registered with the requested
id, and ``hal_flash_init()`` initializes all registered devices, so BSP does
not need to implement anything.
Flash id is the same number that is used as ``device:`` in the BSP flash map
(``bsp.yml``). MCU internal flash uses id 0 (``MCU_FLASH_ID``), other drivers
take id from their own syscfg value (``SPIFLASH_FLASH_ID``, ``QSPI_FLASH_ID``,
``STM32_QSPI_FLASH_ID``, ``IPC_NRF5340_FLASH_ID``, ...). BSP overrides the
value when its flash map uses a different id. Value -1 disables registration.

::

    #if MYNEWT_VAL(SPIFLASH_FLASH_ID) >= 0
    HAL_FLASH_DEV(spiflash, MYNEWT_VAL(SPIFLASH_FLASH_ID), &spiflash_dev.hal);
    #endif

Since table entry is the only reference to the device, package that registers
flash device sets ``pkg.whole_archive: true`` (or adds
``-uhal_flash_dev_<id>_<name>`` to ``pkg.lflags``) so linker keeps the object
file.

BSP that provides its own ``hal_bsp_flash_dev()`` (i.e. for flash devices
created at runtime) sets ``HAL_FLASH_DEV_TABLE: 0``; ``hal_flash_init()`` then
calls ``hal_bsp_flash_dev()`` for consecutive ids as before.

Examples
~~~~~~~~

MCU internal flash of nRF MCUs is registered in
`hal\_flash.c <https://github.com/apache/mynewt-core/blob/master/hw/mcu/nordic/nrf_common/src/hal_flash.c>`__
next to the device definition:

::

    const struct hal_flash nrf_flash_dev = {
        .hf_itf = &nrf_flash_funcs,
        .hf_base_addr = NRF_MEMORY_FLASH_BASE,
        .hf_size = NRF_MEMORY_FLASH_SIZE,
        .hf_sector_cnt = NRF_FLASH_SECTOR_CNT,
        .hf_align = MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE),
        .hf_erased_val = 0xff,
    };

    #if MYNEWT_VAL(MCU_FLASH_ID) >= 0
    HAL_FLASH_DEV(nrf_flash, MYNEWT_VAL(MCU_FLASH_ID), &nrf_flash_dev);
    #endif

Old way, before flash devices were registered with ``HAL_FLASH_DEV()``, was
that every BSP implemented ``hal_bsp_flash_dev()`` and mapped ids to devices
itself. It is still possible with ``HAL_FLASH_DEV_TABLE: 0`` in BSP
``syscfg.yml``, as in
`hal\_bsp.c <https://github.com/apache/mynewt-core/blob/master/hw/bsp/native-armv7/src/hal_bsp.c>`__
of native-armv7 BSP:

::

    const struct hal_flash *
    hal_bsp_flash_dev(uint8_t id)
    {
        /*
         * Just one to start with
         */
        if (id != 0) {
            return NULL;
        }
        return &native_flash_dev;
    }
