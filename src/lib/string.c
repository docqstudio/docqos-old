#include <lib/string.h>
#include <core/const.h>

void *memcpy(void *to,const void *from,int n) /*NOTE: It's easy enough,but it's also slow enough.*/
{
   asm volatile (
      "cld\n\t"
      "cmpq $0,%%rcx\n\t"
      "je 2f\n\t"
      "rep; movsl\n\t"
      "2:" /*label next*/
      "movq %%rax,%%rcx\n\t"
      "andq $0x3,%%rcx\n\t"
      "jz 1f\n\t"
      "rep; movsb\n\t"
      "1:" /*label end*/
      :
      : "c" (n / 4), "a" (n), "D" (to), "S" (from)
       /* n/4 -> %rcx , n -> %rax , to -> %rdi , from -> %rsi*/
      :"memory");
   return to;
}

int memcmp(const void *str1,const void *str2,int n)
{
   register const u8 *s1 = (typeof(s1)) str1;
   register const u8 *s2 = (typeof(s1)) str2;
   register unsigned char c1,c2;
   if(!n)
      return 0;
   do{
      c1 = *s1++;
      c2 = *s2++;
   }while(c1 == c2 && --n);

   return c1 - c2;
}

void *memset(void *mem,u8 c,u64 len)
{
   if(len >= 8)
   {
      u64 *mem64;
      u64 fill = c;
      fill |= fill << 8;
      fill |= fill << 16;
      fill |= fill << 32;

      while((pointer)mem % 8)
      {
         *((u8 *)mem) = c;
         mem += 1;
         len -= 1;
      }

      mem64 = (u64 *)mem;
      u64 loop = len / 64;
      while(loop > 0)
      {
         mem64[0] = mem64[1] = mem64[2] = mem64[3]
            = mem64[4] = mem64[5] = mem64[6] = mem64[7] = fill;
         mem64 += 8;
         loop -= 1;
      }
      len %= 64;

      loop = len / 8;
      while(loop > 0)
      {
         mem64[0] = fill;
         mem64 += 1;
         loop -= 1;
      }
      len %= 8;

      mem = (u8 *)mem64;
   }

   while(len > 0)
   {
      *(u8 *)mem = c;
      --len;
      ++mem;
   }
   return mem;
}

char *itoa(long long val, char *buf, unsigned int radix,
   char alignType,char alignChar,char isUnsigned)
/* alignType = 0  no align
 * alignType > 0 align:left,complete -alignType bits with alignChar
 * alignType < 0 align:right,complete alignType bits with alignChar*/
{
   unsigned int bit = 0;
   char *end;
   char *start;
   if(!alignChar)
      alignChar = ' '; /*The default value of alignChar is ' '.*/
   if((val < 0) && !isUnsigned)
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
   if(alignType > 0)
   {
      while((int)(buf - start) < alignType)
         *(buf++) = alignChar;
   }
   end = buf;
   *(buf--) = '\0';
   do{
      char temp = *buf;
      *buf = *start;
      *start = temp;
      --buf;++start;
   }while(start < buf);
   if(alignType < 0)
   {
      alignType = -alignType;
      while((int)(end - start + 1) < alignType)
         *(end++) = alignChar;
      *end = '\0';
   }

   return end;
}

int strlen(const char *string)
{
   const char *charString;
   const u32 *dwordString;
   u32 dword;

   for(charString = string;(((pointer)(string)) & (sizeof(u32) - 1)) != 0;++charString)
   {
      if(*charString == '\0')
         return charString - string;
   }
   dwordString = (const u32 *)string;

   for(;;)
   {
      dword = *(dwordString++);
      if((((dword + 0x7efefeffL) ^~dword) & (~ 0x7efefeffL)) != 0)
      {
         charString = (const char *)(dwordString - 1);
         if(charString[0] == '\0')
            return charString - string + 0;
         if(charString[1] == '\0')
            return charString - string + 1;
         if(charString[2] == '\0')
            return charString - string + 2;
         if(charString[3] == '\0')
            return charString - string + 3;
      }
   }
}
