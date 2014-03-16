#pragma once
#include <core/const.h>
#include <task/semaphore.h>

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
typedef struct VirtualMemoryArea VirtualMemoryArea;

typedef struct TaskMemory{
   void *page;
   AtomicType ref;
   Semaphore *wait;

   VirtualMemoryArea *vm;
   VFSFile *exec;
} TaskMemory;

typedef struct VirtualMemoryArea {
   VirtualMemoryArea *next;
   VFSFile *file;
   u64 offset;
   u64 start;
   u64 length;
   int prot;
} VirtualMemoryArea;

#define MAP_FIXED     0x01
#define MAP_ANONYMOUS 0x02
#define MAP_PRIVATE   0x04

#define PROT_NONE  0x00
#define PROT_READ  0x01
#define PROT_WRITE 0x02
#define PROT_EXEC  0x04

int doMMap(VFSFile *file,u64 offset,pointer address,u64 len,
          int prot,int flags);

int initPaging(void);

inline int pagingFlushTLB(void) __attribute__ ((always_inline));

inline int pagingFlushTLB(void)
{
   asm volatile("movq %%cr3,%%rax;movq %%rax,%%cr3":::"memory");
   return 0;
}
