MEMORY
{
    FLASH (rx)  : ORIGIN = (0x16008000), LENGTH = (384 * 1024)
    RAM (rw)    : ORIGIN = (0x20000000), LENGTH = (512 * 1024)
}

/* This linker script is used for images and thus contains an image header */
_imghdr_size = 0x20;
