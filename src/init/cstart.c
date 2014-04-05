#include <core/const.h>
#include <video/framebuffer.h>
#include <video/console.h>
#include <memory/memory.h>
#include <memory/paging.h>
#include <lib/string.h>
#include <cpu/cpuid.h>
#include <cpu/gdt.h>
#include <acpi/acpi.h>
#include <interrupt/interrupt.h>
#include <interrupt/idt.h>
#include <time/time.h>
#include <time/localtime.h>
#include <task/task.h>
#include <task/elf64.h>
#include <driver/driver.h>
#include <driver/pci.h>
#include <filesystem/virtual.h>
#include <init/multiboot.h>
#include <driver/serialport.h>

extern void *endAddressOfKernel;

extern InitcallFunction initcallStart;
extern InitcallFunction initcallEnd;

extern int calcMemorySize(MultibootTagMemoryMap *map);

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
   char buf[25] = {};
   printk("\nTry to open file 'test1.c'.\n");
   int fd = doOpen("test1.c",O_RDONLY);
   if(fd < 0)
      printkInColor(0xff,0x00,0x00,"Failed!!This file doesn't exist.\n");
   else
      doClose(fd); /*Never happen.*/

   printk("Try to open file '/dev/dev.inf'.\n");
   fd = doOpen("/dev/dev.inf",O_RDONLY);
   if(fd >= 0)
   {
      printkInColor(0x00,0xff,0x00,"Successful!Read data from it:\n");
      int size = doRead(fd,buf,sizeof(buf) - 1);
      doClose(fd); /*Close it.*/
      buf[size] = '\0';
      printkInColor(0x00,0xff,0xff,"%s\n",buf);
   }
   printk("Run 'mount -t devfs devfs /dev'.\n");
   doMount("/dev",lookForFileSystem("devfs"),0,0);
   printk("Next,check if '/dev/cdrom0' exists:");
   if(openBlockDeviceFile("/dev/cdrom0"))
      printkInColor(0x00,0xff,0x00,"Yes!\n\n");
   else
      printkInColor(0xff,0x00,0x00,"No!\n\n"); 
   frameBufferFillRect(0x00,0x00,0x00,0,0,1024,768); /*Clear the screen.*/
   frameBufferRefreshLine(0,0);

   asm volatile("int $0xff"::"a"(0),"b"("/sbin/init"),"c"(0),"d"(0));
   for(;;);
}

int kmain(u64 magic,u8 *address)
{
   MultibootTagFrameBuffer *fb = 0;
   MultibootTagMemoryMap *mmap = 0;
   int retval;
   endAddressOfKernel = (void *)(&endAddressOfKernel + 1);

   if(magic != MULTIBOOT2_BOOTLOADER_MAGIC)
      return -EINVAL;

   memcpy((void *)endAddressOfKernel,(const void *)address,*(u32 *)address);
   address = (u8 *)endAddressOfKernel;   /*Copy the information.*/
   endAddressOfKernel += *(u32 *)address;

   for(MultibootTag *__tag = (MultibootTag *)(address + 8);
      __tag->type != MULTIBOOT2_TAG_TYPE_END;
      __tag = (MultibootTag *)((u8 *)__tag + ((__tag->size + 7) & ~7)))
   {
      switch(__tag->type)
      {
      case MULTIBOOT2_TAG_TYPE_FRAMEBUFFER:
         {
            MultibootTagFrameBuffer *tag
               = (MultibootTagFrameBuffer *)__tag;
            fb = tag;
         }
         break;
      case MULTIBOOT2_TAG_TYPE_MEMORYMAP:
         {
            MultibootTagMemoryMap *tag
               = (MultibootTagMemoryMap *)__tag;
            mmap = tag;
         }
      default:
         break;
      }
   }

   if(!fb || !mmap)    /*Return if there is no enough information.*/
      return -EINVAL;

   calcMemorySize(mmap);
   initPaging();

   if((retval = initFrameBuffer(fb)))
      return retval; /*Init Frame Buffer..*/
   initSerialPorts();
 
   printkInColor(0x00,0xFF,0x00, /*Green.*/
      "------------------kmain started------------------\n");
   printk("Initialize VESA successfully.\n");

   initGDT();
   initIDT();

   if((retval = initMemory()))
      return retval; /*Error!*/

   displayCPUBrand();

   initDriver();
   
   if((retval = initACPI()))
      return retval;

   if((retval = initInterrupt()))
      return retval;

   if((retval = initTime()))
      return retval;
   if((retval = initLocalTime()))
      return retval;

   initTask();
   return 0;
}
