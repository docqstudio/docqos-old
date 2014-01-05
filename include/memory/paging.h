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


typedef struct VFSFile VFSFile;

int setPTE(void *__pde,pointer address,pointer p);
void *allocPTE(void *__pde,pointer address);
void *allocPDPTE(void *__pml4e,pointer address);
void *allocPDE(void *__pdpte,pointer address);
void *allocPML4E(void);

int doMMap(VFSFile *file,u64 offset,pointer address,u64 len);

int initPaging(void);
