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
   char buf[256];
   u64 base,limit;
   u32 type;
   buf[0] = '0';buf[1] = 'x';
   printk("\nDisplaying memory information...\n");
   printkInColor(0x00,0xFF,0x00,"Base Address:      Limit:             Type:\n");
   for(int i = 0;i < memoryMap->count; ++i)
   {
      base = memoryMap->entries[i].address;
      limit = memoryMap->entries[i].length;
      type = memoryMap->entries[i].type;
      itoa(base,buf + 2,0x10,16,'0',1);
      printk("%s ",buf);
      itoa(limit,buf + 2,0x10,16,'0',1);
      printk("%s ",buf);
      itoa(type,buf + 2,0x10,8,'0',1);
      printk("%s\n",buf);
   }
   itoa(memorySize,buf + 2,0x10,16,'0',1);
   printkInColor(0x00,0xFF,0xFF,"Memory size: %s bytes",buf);
   itoa(memorySize / 1024 / 1024 + 1,buf,10,0,' ',1);
   printkInColor(0x00,0xFF,0xFF,"(About %s MB).\n",buf);

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

   printk("Obj0:%x Obj1:%x Obj2:%x\n",obj0,obj1,obj2);

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
