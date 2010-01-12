OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)
EXTERN(_start)

PHDRS {
      all PT_LOAD ;
}

SECTIONS
{
  ENTRY(_start)

  /* 1 MB */
  . = 0x100000;

  .text :
  {
    *(.text._start)
    *(.text .text.*);
    *(.rodata .rodata.*);
  } : all

  .data :
  {
    *(.data .data.*);
  }

  .bss :
  {
    *(COMMON);
    *(.bss  .bss.*);
    . = ALIGN(0x80000);
  }

  /DISCARD/ :
  {
    *(.not.GNU-stack)
    *(.comment)
  }
}
