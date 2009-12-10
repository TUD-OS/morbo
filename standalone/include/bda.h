/* -*- Mode: C -*- */

#pragma once

#include <stdint.h>
#include <stdbool.h>

struct bios_data_area {
  uint16_t com_port[4];
  uint16_t lpt_port[4];
  uint16_t equipment;

  /* XXX This struct is incomplete. See
     http://www.bioscentral.com/misc/bda.htm for a complete
     description. */
};

static inline unsigned
serial_ports(struct bios_data_area *bda)
{
  /* XXX At least my test box always returns 0, so this seems not to
     be reliable. */
  return (bda->equipment >> 9) & 0x7;
}


static inline struct bios_data_area *
get_bios_data_area(void)
{
  return (struct bios_data_area *)0x400;
}

/* EOF */
