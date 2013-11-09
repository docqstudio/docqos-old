#include <lib/string.h>
#include <core/const.h>

void *memcpy(void *to,const void *from,int n) /*NOTE: It's easy enough,but it's also slow enough.*/
{
   asm volatile (
      "cmpq $0,%%rcx\n\t"
      "je 2f\n\t"
      "rep; movsl\n\t"
      "2:"
      "movq %%rax,%%rcx\n\t"
      "andq $0x3,%%rcx\n\t"
      "jz 1f\n\t"
      "rep; movsb\n\t"
      "1:" /*label end*/
      :
      : "c" (n / 4), "a" (n), "D" (to), "S" (from)
       /* n/4 -> %rcx , n -> %rax , to -> %rdi , from -> %rsi*/
      :"memory");
}

char *itoa(int val, char *buf, unsigned int radix)
{
   unsigned int bit = 0;
   char *end;
   char *start;
   if(val < 0)
   {
      *(buf++) = '-';
      val = -val;
   }
   start = buf;

   do{
      bit = val % radix;
      val /= radix;

      if(bit > 9)
         *(buf++) = (char)(bit - 10 + 'a');
      else
         *(buf++) = (char)(bit - 0 + '0');
   }while(val > 0);
   end = buf;
   *(buf--) = '\0';
   do{
      char temp = *buf;
      *buf = *start;
      *start = temp;
      --buf;++start;
   }while(start < buf);

   return end;
}
