#include <core/const.h>
#include <lib/string.h>
#include <memory/memory.h>
#include <video/console.h>

/*Saved in loader.*/
#define MEMORY_INFO_NUMBER_ADDRESS 0x70000
#define MEMORY_INFO_ADDRESS        0x70010
#define MEMORY_INFO_MAX_NUMBER     15

static MemoryARDS memoryInformation[MEMORY_INFO_MAX_NUMBER] = {};
static u32 memoryInformationNumber = 0;
static u64 memorySize = 0;

static int displayMemoryInformation(void)
{
   char buf[256];
   u64 base,limit,temp;
   u32 type;
   buf[0] = '0';buf[1] = 'x';
   printk("\nDisplaying memory information...\n");
   printkInColor(0x00,0xFF,0x00,"Base Address:      Limit:             Type:\n");
   for(int i = 0;i < memoryInformationNumber; ++i)
   {
      base = (u64)(memoryInformation[i].baseAddrLow);
      base |= ((u64)(memoryInformation[i].baseAddrHigh) << 32);
      limit = (u64)(memoryInformation[i].lengthLow);
      limit |= ((u64)(memoryInformation[i].baseAddrHigh) << 32);
      type = memoryInformation[i].type;
      itoa(base,buf + 2,0x10,16,'0',1);
      printk("%s ",buf);
      itoa(limit,buf + 2,0x10,16,'0',1);
      printk("%s ",buf);
      itoa(type,buf + 2,0x10,8,'0',1);
      printk("%s\n",buf);
      if(type == 0x1)
      {
         temp = base + limit;
         if(temp > memorySize)
            memorySize = temp;
      }
   }
   itoa(memorySize,buf + 2,0x10,16,'0',1);
   printkInColor(0x00,0xFF,0xFF,"Memory size: %s bytes",buf);
   itoa(memorySize / 1024 / 1024 + 1,buf,10,0,' ',1);
   printkInColor(0x00,0xFF,0xFF,"(About %s MB).\n",buf);

   return 0;
}

int initMemory(void)
{
   memoryInformationNumber =
         *(u32 *)(MEMORY_INFO_NUMBER_ADDRESS);
   /*After this ,memoryInformationNumber shoudn't be changed.*/
   if((memoryInformationNumber == 0) ||
      (memoryInformationNumber > MEMORY_INFO_MAX_NUMBER))
   {
      printkInColor(0x0FF,0x00,0x00, /*Red.*/
      "Memory information is too much or few!\n");
#ifdef CONFIG_DEBUG
      printkInColor(0xFF,0x00,0x00,
      "(DEBUG) memoryInformationNumber = %d.\n",memoryInformationNumber);
#endif /*CONFIG_DEBUG*/
      return -1;
   }
   memcpy((void *)memoryInformation, /*to*/
   (const void *)MEMORY_INFO_ADDRESS, /*from*/
   memoryInformationNumber * sizeof(MemoryARDS) /*n*/);
   displayMemoryInformation();
   printk("\n");
   return 0;
}
