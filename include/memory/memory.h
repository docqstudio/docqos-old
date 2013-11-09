#pragma once
#include <core/const.h>

typedef struct MemoryARDS {
   u32 baseAddrLow;
   u32 baseAddrHigh;
   u32 lengthLow;
   u32 lengthHigh;
   u32 type;
} __attribute__ ((packed)) MemoryARDS;

int initMemory(void);
