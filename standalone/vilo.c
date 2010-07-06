/* -*- Mode: C -*- */

/* TODO: Inform BIOS via int 10 of new text cursor position. */

#include <stdint.h>
#include <serial.h>
#include <mbi.h>
#include <util.h>
#include <version.h>

enum {
  SETUP_SECTS =  0x1F1U,
  MAGIC       =  0x1FEU,
  SETUP_CODE  =  0x7C00U, /* setup starts here */
  SYS_CODE    = 0x10000U, /* system loaded at 0x10000 (65536). */

};


int
main(uint32_t magic, struct mbi *mbi)
{
  serial_init();

  /* Command line parsing */
  if (magic != MBI_MAGIC) {
    printf("Not loaded by multiboot loader.\n");
    return 1;
  }

  printf("\nVILO %s\n", version_str);
  printf("Blame Julian Stecklina <jsteckli@os.inf.tu-dresden.de> for bugs.\n\n");

  printf("*** VILO is only intended to load GPXE! ***\n");

  if (mbi->mods_count == 0) {
    printf("No module given. Nothing to do.\n");
    return 1;
  }

  struct module *m  = (struct module *) mbi->mods_addr;
  const char *linux_image = (char *) m->mod_start;
  size_t linux_image_len  = m->mod_end - m->mod_start;

  printf("Module @ %p (%zd bytes)\n", linux_image, linux_image_len);

  uint16_t lmagic = linux_image[MAGIC] | (linux_image[MAGIC+1] << 8);
  if (lmagic != 0xAA55) {
    printf("Magic is wrong %x (should be 0xAA55)\n", lmagic);
    return 1;
  }

  uint8_t setup_sects = linux_image[SETUP_SECTS] + 1 /* Where does +1 come from? */;
  if (setup_sects < 4) {
    printf("Too few setup vectors: %u\n", setup_sects);
    return 3;
  }

  printf("%u setup sectors.\n", setup_sects);

  memcpy((char *)SETUP_CODE, linux_image, setup_sects*512);

  if (SYS_CODE + linux_image_len > 0xA0000) {
    printf("Image to large?\n");
    return 1;
  }

  memcpy((char *)SYS_CODE,   linux_image + setup_sects*512, linux_image_len - setup_sects*512);

  extern void torealmode(uint32_t addr) __attribute__((regparm(3)));
  torealmode(SETUP_CODE + 0x200);
  __builtin_trap();
}
