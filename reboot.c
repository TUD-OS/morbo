/* Linux code. GPL'd. */

#include <asm.h>
#include <util.h>

static inline void
kb_wait(void)
{
  for (int i = 0; i < 0x10000; i++) {
    if ((inb(0x64) & 0x02) == 0)
      break;
    wait(1);
  }
}

void
reboot(void)
{
  out_string("Reset!\n");
  for (int i = 0; i < 10; i++) {
    kb_wait();
    outb(0x64, 0xFE); /* pulse reset low */
    wait(1);
  }

  /* If we get here, reboot didn't work... */
  out_string("Reboot didn't work.\n");
  while (1) {}
}

/* EOF */
