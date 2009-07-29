asm(".code16gcc\n");

extern void int10_putc(char c) __attribute__((regparm(3)));
extern char product_name[];

void
puts(const char *msg)
{
  for (unsigned i = 0; msg[i] != 0; i++)
    int10_putc(msg[i]);
}


void
_init(void)
{
  puts("IEEE1394: INIT\r\n");


  puts("IEEE1394: DONE\r\n");
}

void
_bev(void)
{
  puts("IEEE1394: BOOT!\r\n");

  puts("IEEE1394: FAILED\r\n");
}
