#include <video/vesa.h>
#include <video/console.h>
#include <memory/memory.h>
#include <lib/string.h>

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
   
   writeStringInColor(0x00,0xFF,0x00, /*Green.*/
      "------------------kmain started------------------\n");
   writeString("Initialize VESA sucessfully.\n");

   if(!initMemory()) 
      return -1; /*Error!*/
   return 0;
}
