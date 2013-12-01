#pragma once
#include <core/list.h>
#include <memory/paging.h>

typedef enum PhysicsPageFlags{
   PageReserved = (1 << 0),
   PageData     = (1 << 1),
   PageSlab     = (1 << 2)
} PhysicsPageFlags;

typedef struct PhysicsPage{
   ListHead list;
   PhysicsPageFlags flags;
   u32 count; /*Should use AtomicType,but ...*/
   u64 data;
} PhysicsPage;

inline void *getPhysicsPageAddress(PhysicsPage *page)
   __attribute__ ((always_inline));

inline PhysicsPage *getPhysicsPage(void *obj)
   __attribute__ ((always_inline));

int initBuddySystem(void);

int freePages(PhysicsPage *page,unsigned int order);
PhysicsPage *allocPages(unsigned int order);

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