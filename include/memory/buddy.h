#pragma once
#include <core/list.h>
#include <cpu/atomic.h>
#include <memory/paging.h>

#define PHYSICS_PAGE_SIZE    0x1000

typedef struct PageCache PageCache;

typedef enum PhysicsPageFlags{
   PageReserved = (1 << 0),
   PageData     = (1 << 1),
   PageSlab     = (1 << 2),
   PagePageCache= (1 << 3)
} PhysicsPageFlags;

typedef struct PhysicsPage{
   ListHead list;
   PhysicsPageFlags flags;
   AtomicType count;
   u64 data;
   PageCache *cache;
} PhysicsPage;

inline void *getPhysicsPageAddress(PhysicsPage *page)
   __attribute__ ((always_inline));

inline PhysicsPage *getPhysicsPage(void *obj)
   __attribute__ ((always_inline));

inline PhysicsPage *referencePage(PhysicsPage *page)
   __attribute__ ((always_inline));

int initBuddySystem(void);

int freePages(PhysicsPage *page,unsigned int order);
PhysicsPage *allocPages(unsigned int order);
PhysicsPage *allocAlignedPages(unsigned int order);
PhysicsPage *allocDMAPages(unsigned int order,unsigned int max);
int dereferencePage(PhysicsPage *page,unsigned int order);

u64 getPhysicsPageCount(void);
PhysicsPage *getMemoryMap(void);

inline void *getPhysicsPageAddress(PhysicsPage *page)
{
   PhysicsPage *memoryMap = getMemoryMap();
   void *physicsAddress = 
      (void *)(pointer)(((u64)(page - memoryMap))*0x1000);
   return pa2va(physicsAddress);
}

inline PhysicsPage *getPhysicsPage(void *obj)
{
   PhysicsPage *memoryMap = getMemoryMap();
   pointer physicsObj = va2pa(obj);
   return (memoryMap + ((physicsObj) >> (3*4)));
}

inline PhysicsPage *referencePage(PhysicsPage *page)
{
   atomicAdd(&page->count,1);
   return page;
}
