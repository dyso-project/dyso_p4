#pragma once

// #include <x86intrin.h>
#include <stdio.h>
#include <stdint.h>

// width=32 poly=0x04c11db7 init=0xffffffff refin=false refout=false xorout=0x00000000 check=0x0376e6e7 residue=0x00000000 name="CRC-32/MPEG-2"
inline static uint32_t crc32_mpeg(uint8_t *val, int l) {
   int i, j;
   uint32_t crc, msb;

   crc = 0xFFFFFFFF;
   for(i = 0; i < l; i++) {
      // xor next byte to upper bits of crc
      crc ^= (((uint32_t)val[i])<<24);
      for (j = 0; j < 8; j++) {    // Do eight times.
            msb = crc>>31;
            crc <<= 1;
            crc ^= (0 - msb) & 0x04C11DB7;
      }
   }
   // don't complement crc on output
   return crc;         
}
