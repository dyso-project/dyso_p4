#pragma once

// #include <x86intrin.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

inline static uint32_t crc32_sw(const uint8_t *s,size_t n) {
	uint32_t crc=0xFFFFFFFF;
	
	for(size_t i=0;i<n;i++) {
		uint8_t ch=s[i];
		for(size_t j=0;j<8;j++) {
			uint32_t b=(ch^crc)&1;
			crc>>=1;
			if(b) crc=crc^0xEDB88320;
			ch>>=1;
		}
	}
	
	return ~crc;
}