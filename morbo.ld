OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)
EXTERN(_start)

PHDRS {
      text PT_LOAD ;
      data PT_LOAD ;
}

SECTIONS
{
  ENTRY(_start)

  /* 1 MB */
  . = 0x100000;

  .text :
  {
    *(.text._start)
    *(.text .text.* .gnu.linkonce.t.* );
  } : text

  .data :
  {
      *(.rodata .rodata.* .gnu.linkonce.r.*);
  } : data

  .bss :
  {
     *(.bss  .bss.* .gnu.linkonce.b.*);
  } : data

  /DISCARD/ :
  {
    *(.not.GNU-stack)
    *(.comment)
  }
}
