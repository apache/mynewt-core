MEMORY
{
    /* 
     * Flash is remapped at 0x0 by 1st stage bootloader, but this is done with
     * an offset derived from image header thus it is safer to use remapped
     * address space at 0x0 instead of QSPI_M address space at 0x16000000.
     * Bootloader partition is 32K, but 9K is currently reserved for product
     * header (8K) and image header (1K).
     * First 512 bytes of SYSRAM are remapped at 0x0 and used as ISR vector
     * (there's no need to reallocate ISR vector) and thus cannot be used by
     * application.
     */

    FLASH (rx)  : ORIGIN = (0x00000000), LENGTH = (23 * 1024)
    RAM (rw)    : ORIGIN = (0x20000200), LENGTH = (512 * 1024 - 512)
}

/* The bootloader does not contain an image header */
_imghdr_size = 0x0;
