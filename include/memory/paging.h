#pragma once
#include <core/const.h>

#define PAGE_OFFSET 0x8000000000

#define pa2va(pa) ({ \
   u8 *__ = (u8 *)(pointer)(pa);\
   (void *)(__ + PAGE_OFFSET);\
})
#define va2pa(va) ({ \
   u8 *__ = (u8 *)(va);\
   (pointer)(__ - PAGE_OFFSET);\
})
int initPaging(void);
