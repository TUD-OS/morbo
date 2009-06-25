/* -*- Mode: C -*- */

#include <stdint.h>
#include <crc16.h>

/** Calculate a CRC16 the CSR way.
 * Example from IEEE 1212-2001.
 */
static inline uint16_t
crc16_step(uint32_t crc, uint32_t data)
{
  uint32_t sum;

  for (int shift = 28; shift >= 0; shift -= 4) {
    sum = ((crc >> 12) ^ (data >> shift)) & 0xF;
    crc = (crc << 4) ^ (sum << 12) ^ (sum << 5) ^ sum;
  }

  return (uint16_t)crc;
}


uint16_t
crc16(uint32_t *data, size_t length)
{
  uint16_t crc = 0;

  for (size_t i = 0; i < length; i++)
    crc = crc16_step(crc, data[i]);

  return crc;
}

/* EOF */
