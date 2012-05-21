/* -*- Mode: C -*- */
/*
 * IEEE1394 Config ROM definitions.
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

/* The config ROM looks like
   
       most sig 
   0  |   bus_info_length |   crclength       |                  crc                  |
   1  |                                     bus name                                  |
   2  |                                bus dependent info                             |
   3  |                                      EUI64                                    |
   4  |                                      EUI64                                    |
   n                              ....bus dependent info ....
   n+1|             root length               |              root_crc                 |

   root length includes everything from the bus name until the end of
   the bus dependend info.

 */

#define CONFIG_ROM_SIZE 0x400

typedef struct __attribute__((__packed__)) {
  uint32_t field[CONFIG_ROM_SIZE];
} ohci_config_rom_t;

/* EOF */
