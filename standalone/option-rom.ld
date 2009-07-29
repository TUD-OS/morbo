OUTPUT_FORMAT("binary")

PHDRS {
      text PT_LOAD ;
      data PT_LOAD ;
}

SECTIONS
{
  /* 1 MB */
  . = 0;

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
