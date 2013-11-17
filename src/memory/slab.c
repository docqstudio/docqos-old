#include <core/const.h>
#include <core/list.h>
#include <memory/slab.h>
#include <memory/buddy.h>
#include <video/console.h>

#define LOCAL_SLAB_CACHE_BATCH_COUNT_DEFAULT 0x010
#define LOCAL_SLAB_CACHE_DATA_COUNT_DEFAULT  0x050
#define OBJECT_COUNT_PER_SLAB_DEFAULT        0x100

typedef struct StaticLocalSlabCache{
   LocalSlabCache localSlabCache;
   void *data[LOCAL_SLAB_CACHE_DATA_COUNT_DEFAULT];
} __attribute__ ((packed)) StaticLocalSlabCache;


/*Init data.But after init,it will still be used.*/
static SlabCache cacheCache = {}; /*Use for creating SlabCache.*/
static SlabCache localCacheCache = {}; /*Use for creating StaticLocalSlabCache.*/
static StaticLocalSlabCache staticLocalSlabCache[2] = {
   {{LOCAL_SLAB_CACHE_DATA_COUNT_DEFAULT,0,0x10},{0}}, /*Used by cacheCache.*/
   {{LOCAL_SLAB_CACHE_DATA_COUNT_DEFAULT,0,0x10},{0}} /*Used by localCacheCache.*/
   };

static PhysicsPage *slabAllocPages(SlabCache *cache)
{
   PhysicsPage *page,*tmp;
   u64 i = (1ul << cache->perSlabOrder);
   page = tmp = allocPages(cache->perSlabOrder); /*From Buddy System.*/
   if(!page)
      return 0;
   while(i--)
      (page++)->flags |= PageSlab;
   return tmp;
}

static int slabFreePages(SlabCache *cache,PhysicsPage *page)
{
   u64 i = (1ul << cache->perSlabOrder);
   PhysicsPage *tmp = page;
   while(i--)
      (tmp++)->flags &= ~PageSlab;
   freePages(page,cache->perSlabOrder); /*To Buddy System.*/
   return 0;
}

static Slab *allocSlabForSlabCache(SlabCache *cache)
{
   PhysicsPage *page = slabAllocPages(cache);
   void *obj = getPhysicsPageAddress(page);
   Slab *slab = (Slab *)obj;
   slab->nextFree = 0;
   slab->usedCount = 0;
   slab->memory = obj + cache->slabSize;

   for(u64 i = 0;i < (1ul << cache->perSlabOrder);++i)
   {
      (page + i)->list.prev = (ListHead *)slab;
      (page + i)->list.next = (ListHead *)cache; 
      /*When the physics pages are not free,page->list will not be used by Buddy System.*/
      /*If the physics pages are used by Slab(page->flags & PageSlab),*/
      /*page->list.prev is slab of the page,page->list.next is cache of the page.*/
   }

   for(int i = 0; i < cache->perSlabObjCount;++i)
   {
      slab->objDescriptor[i] = i + 1; /*Next free index.*/
   }
   slab->objDescriptor[cache->perSlabObjCount - 1] = (SlabObjDescriptor)(-1);
                                                      /*The last.*/
   listAdd(&slab->list,&cache->slabFree);

   cache->freeObjCount += cache->perSlabObjCount;
   return slab;
}

static int destorySlab(SlabCache *cache,Slab *slab)
{
   void *obj = (void *)slab;
   PhysicsPage *memoryMap = getMemoryMap();
   PhysicsPage *page = memoryMap + (((u64)(obj)) >> (4*3));

   cache->freeObjCount -= cache->perSlabObjCount;
   cache->localCache[0]->avail = 0;/*Must refill local cache.*/
   listDelete(&slab->list);
   slabFreePages(cache,page);
   return 0;
}

static SlabCache *initSlabCache(SlabCache *cache,u32 objSize,u32 objCount,
   LocalSlabCache *localCache)
{
   initList(&cache->slabFree);
   initList(&cache->slabPartial);
   initList(&cache->slabFull);

   u32 objDescriptorSize = objCount*sizeof(SlabObjDescriptor);
   u32 slabSize = objDescriptorSize + sizeof(Slab);
   u32 slabPage,slabOrder;

   slabPage = slabSize + objSize * objCount;
   slabPage += 0xfff;
   slabPage >>= 3*4; 

   for(slabOrder = 0;slabOrder < 11;++slabOrder)
      if((1ul << slabOrder) >= slabPage)
         break;

   cache->perSlabOrder = slabOrder;
   cache->perSlabObjCount = objCount;
   cache->objSize = objSize;
   cache->freeObjCount = 0;
   cache->slabSize = slabSize;
   cache->localCache[0] = localCache;
   cache->freeLimit = cache->perSlabObjCount + 2*cache->localCache[0]->batchCount;

   return cache;
}

static void *refillCache(SlabCache *cache)
{
   LocalSlabCache *localCache = cache->localCache[0];
   u32 batchCount = localCache->batchCount + 1;
   if(batchCount == 0)
      return 0;
retry:
   while(batchCount > 0)
   {
      ListHead *list = cache->slabPartial.next;
      if(list == &cache->slabPartial)
      {
         list = cache->slabFree.next;
	 if(list == &cache->slabFree)
	    break;
      }

      Slab *slab = listEntry(list,Slab,list);
      while(slab->usedCount < cache->perSlabObjCount && --batchCount)
      {
         ++slab->usedCount;
	 localCache->data[localCache->avail++] = 
	    slab->memory + slab->nextFree * cache->objSize;
	 slab->nextFree = slab->objDescriptor[slab->nextFree];
	 --cache->freeObjCount;
      }
      listDelete(&slab->list);
      if(slab->nextFree == (u32)-1) /*Full?*/
         listAdd(&slab->list,&cache->slabFull);
      else
         listAdd(&slab->list,&cache->slabPartial);
   }
   if(localCache->avail)
      return localCache->data[--localCache->avail];
   if(!allocSlabForSlabCache(cache))
      return 0; /*Error!*/
   goto retry; 
   return 0;
}

static int flushLocalCache(SlabCache *cache)
{
   LocalSlabCache *localCache = cache->localCache[0];
   u32 batchCount = localCache->batchCount;
   void **objects = localCache->data;
   Slab *slab;
   PhysicsPage *memoryMap = getMemoryMap();
   PhysicsPage *page;
   u32 objnr;

   for(int i = 0;i < batchCount;++i)
   {
      void *obj = objects[i];
      page = memoryMap + (((u64)obj) >> (3*4)); /*Get page index.*/
      slab = (Slab *)page->list.prev; /*Get slab,see also allocSlabForSlabCache.*/
      objnr = (obj - slab->memory) / cache->objSize;

      slab->objDescriptor[objnr] = slab->nextFree;
      slab->nextFree = objnr;

      --slab->usedCount;
      ++cache->freeObjCount;
      listDelete(&slab->list);
      if(!slab->usedCount){ /*Free?*/
         if(cache->freeObjCount > cache->freeLimit)
	    destorySlab(cache,slab);
	 else
            listAdd(&slab->list,&cache->slabFree);
      }else
         listAdd(&slab->list,&cache->slabPartial);
   }
   localCache->avail -= batchCount;
   return 0;
}

void *allocByCache(SlabCache *cache)
{
   LocalSlabCache *localCache = cache->localCache[0];
   void *ret;

   if(localCache->avail)
      ret = localCache->data[--localCache->avail];
   else
      ret = refillCache(cache);
   return ret;
}

int freeByCache(SlabCache *cache,void *obj)
{
   LocalSlabCache *localCache = cache->localCache[0];
   
   if(localCache->avail == localCache->limit)
      flushLocalCache(cache);
   localCache->data[localCache->avail++] = obj;
   return 0;
}

SlabCache *createCache(unsigned int size,unsigned int align)
{
   if(align != 0x0)
   {
      align = (1 << align);
      size += align - 1;
      size &= ~ (align - 1);
   } /*Align size with (1 << align).*/

   SlabCache *cache = allocByCache(&cacheCache);
   if(!cache)
      return 0;
   LocalSlabCache *localCache = allocByCache(&localCacheCache);
   if(!cache)
   {
      freeByCache(&cacheCache,cache);
      return 0;
   }
   localCache->batchCount = LOCAL_SLAB_CACHE_BATCH_COUNT_DEFAULT;
   localCache->avail = 0;
   localCache->limit = LOCAL_SLAB_CACHE_DATA_COUNT_DEFAULT;
   /*Init localCache.*/

   if(!initSlabCache(cache,size,OBJECT_COUNT_PER_SLAB_DEFAULT,localCache))
   {
      freeByCache(&cacheCache,cache);
      freeByCache(&localCacheCache,localCache);
      return 0;
   }
   return cache;
}

int destoryCache(SlabCache *cache)
{
   ListHead *list;
   Slab *slab;
   for(list = cache->slabFree.next;list != &cache->slabFree;list = cache->slabFree.next)
   {
      slab = listEntry(list,Slab,list);
      destorySlab(cache,slab);
   }
   for(list = cache->slabPartial.next;list != &cache->slabPartial;list = cache->slabPartial.next)
   {
      slab = listEntry(list,Slab,list);
      destorySlab(cache,slab);
   }
   for(list = cache->slabFull.next;list != &cache->slabFull;list = cache->slabFull.next)
   {
      slab = listEntry(list,Slab,list);
      destorySlab(cache,slab);
   }
   /*It is not a endless loop,because one of the slabs will be destoried in destorySlab.*/

   LocalSlabCache *localCache = cache->localCache[0];
   freeByCache(&localCacheCache,localCache);

   freeByCache(&cacheCache,cache);

   return 0;
}

int initSlab(void)
{
   initSlabCache(&cacheCache,sizeof(SlabCache),OBJECT_COUNT_PER_SLAB_DEFAULT
      ,&staticLocalSlabCache[0].localSlabCache);
   initSlabCache(&localCacheCache,sizeof(StaticLocalSlabCache),
      OBJECT_COUNT_PER_SLAB_DEFAULT,&staticLocalSlabCache[1].localSlabCache);
   printkInColor(0x00,0xff,0x00,"Initialize slab sucessfully!!\n");
   return 0;
}
