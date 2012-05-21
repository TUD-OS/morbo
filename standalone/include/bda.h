/* -*- Mode: C -*- */
/*
 * BIOS Data Area definitions.
 *
 * Copyright (C) 2009-2012, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Morbo.
 *
 * Morbo is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Morbo is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

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
