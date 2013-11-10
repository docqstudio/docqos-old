#include <core/const.h>
#include <video/vesa.h>
#include <video/console.h>
#include <memory/memory.h>
#include <lib/string.h>
#include <cpu/cpuid.h>
#include <acpi/acpi.h>
#include <interrupt/interrupt.h>

#define GDT_SIZE (6*8)

static u8 gdtr64[2 + 8] = {};
/*2: word limit,8: qword base.*/
static u8 gdt64[GDT_SIZE] = {};

int kmain(void)
{
   asm volatile("sgdt (%%rax)"::"a" (gdtr64):"memory");
   memcpy((void *)gdt64,
      (const void *)(pointer)*(u64 *)(gdtr64 + 2),
      GDT_SIZE);
   *(u64 *)(gdtr64 + 2) = (u64)gdt64;
   asm volatile("lgdt (%%rax)"::"a" (gdtr64):"memory");

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
