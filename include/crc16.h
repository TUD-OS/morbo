/* -*- Mode: C -*- */

#pragma once

#include <stdint.h>

uint16_t crc16_step(uint32_t crc, uint32_t data);
uint16_t crc16(uint32_t *data, unsigned length);

/* EOF */
