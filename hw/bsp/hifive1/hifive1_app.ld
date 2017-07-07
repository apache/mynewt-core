MEMORY
{
  flash (rxai!w) : ORIGIN = 0x20400000, LENGTH = 512M
  ram (wxa!ri) : ORIGIN = 0x80000000, LENGTH = 16K
}

/* This linker script is used for images and thus contains an image header */
_imghdr_size = 0x20;
