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
} VirtualMemoryArea;

int doMMap(VFSFile *file,u64 offset,pointer address,u64 len);

int initPaging(void);

inline int pagingFlushTLB(void) __attribute__ ((always_inline));

inline int pagingFlushTLB(void)
{
   asm volatile("movq %%cr3,%%rax;movq %%rax,%%cr3":::"memory");
   return 0;
}
