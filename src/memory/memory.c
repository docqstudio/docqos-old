#include <core/const.h>
#include <lib/string.h>
#include <memory/memory.h>
#include <memory/buddy.h>
#include <memory/slab.h>
#include <memory/kmalloc.h>
#include <memory/paging.h>
#include <video/console.h>
#include <init/multiboot.h>

/*Saved in loader.*/
#define MEMORY_INFO_NUMBER_ADDRESS (0x70000ul + PAGE_OFFSET)
#define MEMORY_INFO_ADDRESS        (0x70010ul + PAGE_OFFSET)
#define MEMORY_INFO_MAX_NUMBER     15

extern void *endAddressOfKernel; /*Defined in ldscripts/kernel.lds.*/

static MultibootTagMemoryMap *memoryMap;
static u64 memorySize = 0;

static int parseMemoryInformation(void)
{
   u32 startPageIndex,endPageIndex;
   u64 base,limit;
   PhysicsPage *__memoryMap = getMemoryMap();
   pointer endOfKernel = (pointer)endAddressOfKernel;
   endOfKernel = va2pa(endOfKernel);
   for(int i = 0;i < memoryMap->count; ++i)
   {
      base = memoryMap->entries[i].address;
      limit = memoryMap->entries[i].length;
      limit += base;
      if(memoryMap->entries[i].type != 0x1)
         continue;
      if(limit < endOfKernel)
         continue;
      if(base < endOfKernel)
         base = endOfKernel;
      startPageIndex = (base >> (3*4)) + 1;
      endPageIndex = ((limit + 0xfff) >> (3*4)) - 1;
      for(int j = startPageIndex;j < endPageIndex;++j)
      {
         freePages(__memoryMap + j,0);
      }
   }
   return 0;
}

static int displayMemoryInformation(void)
{
   u64 base,limit;
   u32 type;
   printk("\nDisplaying memory information...\n");
   printkInColor(0x00,0xFF,0x00,"Base Address:      Limit:             Type:\n");
   for(int i = 0;i < memoryMap->count; ++i)
   {
      base = memoryMap->entries[i].address;
      limit = memoryMap->entries[i].length;
      type = memoryMap->entries[i].type;
      printk("0x%016lx 0x%016lx 0x%08x\n",base,limit,type);
   }
   printkInColor(0x00,0xFF,0xFF,"Memory size: 0x%016lx bytes",memorySize);
   printkInColor(0x00,0xFF,0xFF,"(About %ld MB).\n",memorySize / 1024 / 1024 + 1);

   return 0;
}

int calcMemorySize(MultibootTagMemoryMap *map)
{
   u64 base,limit,temp;
   u32 type;

   map->count = (map->tag.size - sizeof(*map)) / map->count;
   for(int i = 0;i < map->count;++i)
   {
      base = map->entries[i].address;
      limit = map->entries[i].length;
      type = map->entries[i].type;
      if(type == 0x1)
      {
         temp = base + limit;
         if(temp > memorySize)
            memorySize = temp;
      }
  }
  memoryMap = map;
  return 0;
}

int initMemory(void)
{
   displayMemoryInformation();
   initBuddySystem();
   parseMemoryInformation();
   
   initSlab();
   
   initKMalloc();
   printk("Try to use kmalloc.....\n");

   void *obj1 = kmalloc(48);
   void *obj0 = kmalloc(16);
   void *obj2 = kmalloc(100);

   printk("Obj0:0x%p Obj1:0x%p Obj2:0x%p\n",obj0,obj1,obj2);

   printk("Try to use kfree.....\n");

   kfree(obj1);
   kfree(obj0);
   kfree(obj2);

   printk("Successful!\n");

   printk("\n");
   return 0;
}

u64 getMemorySize(void)
{
   return memorySize;
}
