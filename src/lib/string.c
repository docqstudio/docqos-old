#include <lib/string.h>
#include <core/const.h>

void *memcpy(void *to,const void *from,int n) /*NOTE: It's easy enough,but it's also slow enough.*/
{
   asm volatile ("rep; movsl\n\t"
      "movq %%rax,%%rcx\n\t"
      "andq $0x3,%%rcx\n\t"
      "jz 1f\n\t"
      "rep; movsb\n\t"
      "1:" /*label end*/
      :
      : "c" (n / 4), "a" (n) ,"D" (to), "S" (from)
       /* n/4 -> %rcx , n -> %rax , to -> %rdi , from -> %rsi*/
      :"memory");
}
