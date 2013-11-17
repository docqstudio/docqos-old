#pragma once
#include <core/list.h>

typedef u32 SlabObjDescriptor;
/*It's next free index.*/

typedef struct LocalSlabCache{
   u32 limit;
   u32 avail;
   u32 batchCount;

   void *data[0];
} LocalSlabCache;

typedef struct SlabCache{
   LocalSlabCache *localCache[1];

   unsigned int perSlabOrder;
   unsigned int perSlabObjCount;

   u32 slabSize;
   u32 objSize;

   u32 freeLimit;
   u32 freeObjCount;

   ListHead slabFree;
   ListHead slabFull;
   ListHead slabPartial;
} SlabCache;

typedef struct Slab{
   ListHead list;
   void *memory;
   u32 nextFree;
   u32 usedCount;

   SlabObjDescriptor objDescriptor[0];
} Slab;

int initSlab(void);
void *allocByCache(SlabCache *cache);
int freeByCache(SlabCache *cache,void *obj);
SlabCache *createCache(unsigned int size,unsigned int align);
int destoryCache(SlabCache *cache);
