MEMORY
{
  flash (rxai!w) : ORIGIN = 0x20000000, LENGTH = 32K
  ram (wxa!ri) : ORIGIN = 0x80000000, LENGTH = 16K
}

/* The bootloader does not contain an image header */
_imghdr_size = 0x0;
