#include <core/const.h>
#include <core/list.h>
#include <memory/slab.h>
#include <memory/buddy.h>

#ifdef CONFIG_DEBUG
#include <video/console.h>
#endif

typedef struct MallocSize{
   u32 size;
   SlabCache *cache;
} MallocSize;

static MallocSize mallocSizes[] = {
#define CACHE(x) {.size = x,.cache = 0}
   CACHE(32),
   CACHE(64),
   CACHE(128),
   CACHE(256),
   CACHE(512),
   CACHE(1024),
   CACHE(2048),
   CACHE(4096),
   CACHE(8192),
   CACHE(16384),
   CACHE(32768),
   CACHE(65536),
   CACHE(131072),
   CACHE(262144),
   CACHE(524288),
   CACHE(1048576) /*1MB*/
#undef CACHE
};

int initKMalloc(void)
{
   for(int i = 0;i < sizeof(mallocSizes)/sizeof(MallocSize);++i)
   {
      mallocSizes[i].cache = createCache(mallocSizes[i].size,0x0);
#ifdef CONFIG_DEBUG
      if(!mallocSizes[i].cache)
      {
         printkInColor(0xff,0x00,0x00,"(%s) Can't get memorySize[%d].cache!",__func__,i);
	 return -1;
      }
#endif
   }
   return 0;
}

void *kmalloc(unsigned int size)
{
   for(int i = 0;i < sizeof(mallocSizes)/sizeof(MallocSize);++i)
   {
      if(mallocSizes[i].size < size)
         continue;
      SlabCache *cache = mallocSizes[i].cache;
      return allocByCache(cache);
   }
   return 0;
}

int kfree(const void *obj)
{
   PhysicsPage *memoryMap,*page;
   SlabCache *cache;
   memoryMap = getMemoryMap();
   page = memoryMap + (((u64)obj) >> (3*4));
   cache = (SlabCache *)page->list.next;

#ifdef CONFIG_DEBUG
   if(!cache)
   {
      printkInColor(0xff,0x00,0x00,"(%s) Can't get the cache of the obj %x.",__func__,obj);
      return -1;
   }
#endif

   return freeByCache(cache,(void *)obj);
}
