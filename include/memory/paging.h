#pragma once
#include <core/const.h>

#define PAGE_OFFSET 0x8000000000
#define PAGE_SIZE   0x1000

#define pa2va(pa) ({ \
   u8 *__ = (u8 *)(pointer)(pa);\
   (void *)(__ + PAGE_OFFSET);\
})
#define va2pa(va) ({ \
   u8 *__ = (u8 *)(va);\
   (pointer)(__ - PAGE_OFFSET);\
})


typedef struct VFSFile VFSFile;

int doMMap(VFSFile *file,u64 offset,pointer address,u64 len);

int initPaging(void);
