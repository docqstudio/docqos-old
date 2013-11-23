#include <core/const.h>
#include <video/vesa.h>
#include <video/console.h>
#include <memory/memory.h>
#include <memory/paging.h>
#include <lib/string.h>
#include <cpu/cpuid.h>
#include <acpi/acpi.h>
#include <interrupt/interrupt.h>

#define GDT_SIZE (6*8)

extern void *endAddressOfKernel;

int kmain(void)
{
   endAddressOfKernel = (void *)(&endAddressOfKernel);
   
   initPaging();

   initVESA(); /*Init vesa.*/

   printkInColor(0x00,0xFF,0x00, /*Green.*/
      "------------------kmain started------------------\n");
   printk("Initialize VESA successfully.\n");

   if(initMemory()) 
      return -1; /*Error!*/

   displayCPUBrand();

   if(initACPI())
      return -1;
   if(initInterrupt())
      return -1;
   return 0;
}
