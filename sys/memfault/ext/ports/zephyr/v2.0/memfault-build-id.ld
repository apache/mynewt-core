SECTION_PROLOGUE(.note.gnu.build-id,,)
{
  __start_gnu_build_id_start = .;
  KEEP(*(.note.gnu.build-id))
} GROUP_LINK_IN(ROMABLE_REGION)
