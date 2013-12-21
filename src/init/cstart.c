#include <core/const.h>
#include <video/vesa.h>
#include <video/console.h>
#include <memory/memory.h>
#include <memory/paging.h>
#include <lib/string.h>
#include <cpu/cpuid.h>
#include <cpu/gdt.h>
#include <acpi/acpi.h>
#include <interrupt/interrupt.h>
#include <time/time.h>
#include <time/localtime.h>
#include <task/task.h>
#include <driver/driver.h>
#include <driver/pci.h>

extern void *endAddressOfKernel;

extern InitcallFunction initcallStart;
extern InitcallFunction initcallEnd;

int doInitcalls(void)
{
   InitcallFunction *start = &initcallStart;
   InitcallFunction *end = &initcallEnd;
   int ret = 0;
   printk("\n");
   for(;start < end;++start)
   {
      ret = (*start)();
      if(ret)
         return ret;
   }
   return ret;
}

int kmain(void)
{
   endAddressOfKernel = (void *)(&endAddressOfKernel + 1);

   initPaging();

   initVESA(); /*Init vesa.*/
 
   printkInColor(0x00,0xFF,0x00, /*Green.*/
      "------------------kmain started------------------\n");
   printk("Initialize VESA successfully.\n");

   initGDT();

   if(initMemory()) 
      return -1; /*Error!*/

   displayCPUBrand();

   initDriver();

   if(initACPI())
      return -1;

   if(initInterrupt())
      return -1;

   if(initTime())
      return -1;
   if(initLocalTime())
      return -1;

   initTask();
   return 0;
}
