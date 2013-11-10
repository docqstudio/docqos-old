#include <core/const.h>
#include <cpu/cpuid.h>
#include <video/console.h>

int getCPUID(u32 eaxSet,u32 *eax,u32 *ebx,u32 *ecx,u32 *edx)
{
   asm volatile(
      "cpuid"
      :"=a"(*eax),"=b"(*ebx),"=c"(*ecx),"=d"(*edx)
      :"a"(eaxSet));
   return 0;
}

u8 *getCPUBrand(u8 *buf) 
/*The size of buf should be more than 4*4*3.(4*4*3 is not OK.)*/
{
   u32 *buf32 = (u32 *)buf;
   u32 eax,ebx,ecx,edx;
   int i = 0;
   do{
      getCPUID(0x80000002 + i,&eax,&ebx,&ecx,&edx);
      *(buf32++) = eax;
      *(buf32++) = ebx;
      *(buf32++) = ecx;
      *(buf32++) = edx;
   }while(i++ < 2);
   *(u8 *)buf32 = '\0';
   return (u8 *)buf32;
}

int displayCPUBrand(void)
{
   u8 buf[4*4*3 + 1];
   getCPUBrand(buf);
   printk("CPU Brand: %s.\n\n",(const char *)buf);

   return 0;
}

int checkIfCPUHasApic(void)
{
   u32 eax,ebx,ecx,edx;
   getCPUID(0x00000001,&eax,&ebx,&ecx,&edx);
   return edx & (1 << 9);
}
