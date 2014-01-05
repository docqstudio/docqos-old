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
#include <task/elf64.h>
#include <driver/driver.h>
#include <driver/pci.h>
#include <filesystem/virtual.h>

extern void *endAddressOfKernel;

extern InitcallFunction initcallStart;
extern InitcallFunction initcallEnd;

static int doInitcalls(void)
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

int kinit(void)
{
   doInitcalls();
   char *buf[20] = {};
   printk("\nTry to open file 'test1.c'.\n");
   int fd = doOpen("test1.c");
   if(fd < 0)
      printkInColor(0xff,0x00,0x00,"Failed!!This file doesn't exist.\n");
   else
      doClose(fd); /*Never happen.*/

   printk("Try to open file '/dev/dev.inf'.\n");
   fd = doOpen("/dev/dev.inf");
   if(fd >= 0)
   {
      printkInColor(0x00,0xff,0x00,"Successful!Read data from it:\n");
      int size = doRead(fd,buf,sizeof(buf) - 1);
      doClose(fd); /*Close it.*/
      buf[size] = '\0';
      printkInColor(0x00,0xff,0xff,"%s\n",buf);
   }
   printk("Run 'mount -t devfs devfs /dev'.\n");
   doMount("/dev",lookForFileSystem("devfs"),0);
   printk("Next,check if '/dev/cdrom0' exists:");
   if(openBlockDeviceFile("/dev/cdrom0"))
      printkInColor(0x00,0xff,0x00,"Yes!\n\n");
   else
      printkInColor(0xff,0x00,0x00,"No!\n\n"); 

   asm volatile("int $0xff"::"a"(0),"b"("/sbin/init"));
   for(;;);
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
