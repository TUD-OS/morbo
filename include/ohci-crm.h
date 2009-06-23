/* -*- Mode: C -*- */

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
