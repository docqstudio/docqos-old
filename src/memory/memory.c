#include <core/const.h>
#include <memory/memory.h>
#include <video/console.h>

/*Saved in loader.*/
#define MEMORY_INFO_NUMBER_ADDRESS 0x70000
#define MEMORY_INFO_ADDRESS        0x70010
#define MEMORY_INFO_MAX_NUMBER     15

static MemoryARDS memoryInformation[MEMORY_INFO_MAX_NUMBER] = {};
static u32 memoryInformationNumber = 0;

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
   return 0;
}
int displayMemoryInformation(void)
{
   char buf[256];
   buf[0] = '0';buf[1] = 'x';
   MemoryARDS info;
   printk("\nDisplaying memory information...\n");
   printkInColor(0x00,0xFF,0x00,"BaseAddrL  BaseAddrH  LengthLow  LengthHigh   Type\n");
   for(int i = 0;i < memoryInformationNumber; ++i)
   {
      info = memoryInformation[i];
      itoa(info.baseAddrLow,buf + 2,0x10,8,'0',1);
      printk("%s ",buf);
      itoa(info.baseAddrHigh,buf + 2,0x10,8,'0',1);
      printk("%s ",buf);
      itoa(info.lengthHigh,buf + 2,0x10,8,'0',1);
      printk("%s ",buf);
      itoa(info.lengthLow,buf + 2,0x10,8,'0',1);
      printk("%s ",buf);
      itoa(info.type,buf + 2,0x10,8,'0',1);
      printk("%s\n",buf);
   }
   return 0;
}
