#include <core/const.h>
#include <core/math.h>
#include <memory/user.h>

               /*This macro is used to make a getUserX function.*/
#define makeGettingUser(address,out,error,type,ax) \
   asm volatile( \
      "0: mov" type " (%%rax),%%" ax "\n\t" \
      "   xorq %%rcx,%%rcx\n\t" /*No errors.*/ \
      "2: \n\t" \
      ".section \".fixup\",\"a\"\n\t" \
      ".quad 0b,2b\n\t" /*Fix up table,see handleException.*/ \
      ".previous\n\t" /*Previous section.*/ \
      : "=a"(out),"=c" (error) \
      : "a"(address),"c" (-EFAULT) \
   )
             /*This macro is used to make a putUserX function.*/
#define makePuttingUser(address,in,error,type,ax) \
   asm volatile( \
      "0: mov" type " %%" ax ",(%%rbx) \n\t" \
      "   xorq %%rcx,%%rcx\n\t" \
      "2: \n\t" \
      ".section \".fixup\",\"a\"\n\t" \
      ".quad 0b,2b\n\t" \
      ".previous\n\t" \
      : "=c" (error) \
      : "c" (-EFAULT),"b" (address),"a" (in) \
   )

       /*This macro makes rep;movsX asm code (for memcpy)....*/
          /*Maybe rep;cmpsX will be also needed in the future...*/
#define repmovsx(type,times,src,dest,error) \
   asm volatile( \
      "0: rep;movs" type " \n\t" \
      "   xorq %%rax,%%rax\n\t" \
      "2: \n\t" \
      ".section \".fixup\",\"a\"\n\t" \
      ".quad 0b,2b\n\t" \
      ".previous\n\t" \
      : "=a" (error),"=S"(src),"=D"(dest) \
      : "a" (-EFAULT),"S"(src),"D"(dest),"c"(times) \
   );

static int memcpyUserCommon(void *dest,const void *src,unsigned long n)
{
   unsigned long error = 0;
   unsigned long align;
   unsigned long how;
   if((align = (((unsigned long)dest) & 7)) != (((unsigned long)src) & 7))
      goto slow; /*The dest and src is aligned?*/
   if(n < 8)
      goto slow; /*Too few data....*/
   if(align && (n - 8 + align) < 8)
      goto slow;

   if(align && (align = 8 - align,n -= align,1))
      repmovsx("b",align,src,dest,error); /*Align the data...*/
   if(error)
      return error;

   how = n >> 3;
   repmovsx("q",how,src,dest,error); /*Copy the data fast.*/
   if(error)
      return error;

   n &= 7;
slow:
   repmovsx("b",n,src,dest,error); /*Copy the data slowly.*/
   return 0;
}


int getUser8(UserSpace(const void) *address,unsigned char *retval)
{
   int error;
   makeGettingUser(address,*retval,error,"b","al");
   return error;
}

int putUser8(UserSpace(void) *address,unsigned char content)
{
   int error;
   makePuttingUser(address,content,error,"b","al");
   return error;
}

int getUser32(UserSpace(const void) *address,unsigned int *retval)
{
   int error;
   makeGettingUser(address,*retval,error,"l","eax");
   return error;
}

int putUser32(UserSpace(void) *address,unsigned int content)
{
   int error;
   makePuttingUser(address,content,error,"l","eax");
   return error;
}

int getUser64(UserSpace(const void) *address,unsigned long *retval)
{
   int error;
   makeGettingUser(address,*retval,error,"q","rax");
   return error;
}

int putUser64(UserSpace(void) *address,unsigned long content)
{
   int error;
   makePuttingUser(address,content,error,"q","rax");
   return error;
}

int memcpyUser0(UserSpace(void) *dest,const void *src,unsigned long n)
{
   if(verifyUserAddress(dest,n))
      return -EFAULT; /*Verify the address and call the common function.*/
   return memcpyUserCommon((void *)dest,(const void *)src,n);
}

int memcpyUser1(void *dest,UserSpace(const void) *src,unsigned long n)
{
   if(verifyUserAddress(src,n))
      return -EFAULT;
   return memcpyUserCommon((void *)dest,(const void *)src,n);
}

long strncpyUser1(char *dest,UserSpace(const char) *src,unsigned long n)
{
   unsigned long align;
   unsigned long how;
   unsigned long data;
   unsigned char data8;
   char *__dest = dest;

   if(verifyUserAddress(src,1))
      return -EFAULT; /*Verify the address.*/
   n = min(getCurrentTask()->addressLimit - (unsigned long)src,n);
   --n; /*For '\0' in the end.*/

   if((align = (((unsigned long)dest) & 7)) != (((unsigned long)src) & 7))
      goto slow; /*The data isn't aligned,we can only use the slow version.*/
   if(n < 8)
      goto slow;
   if(align)
      align = 8 - align;
   if(align && (n - align) < 8)
      goto slow;

   if(align)
      for(int i = 0;i < align;++align)
         if(getUser8(src++,&data8)) /*Get the data.*/
            return -EFAULT;
         else if(!data8)
            goto done;
         else
            *dest++ = data8; /*Copy to the dest[in kernel].*/

   how = n >> 3;
   for(int i = 0;i < how;++i,src += 8,dest += 8,n -= 8)
   {
      if(getUser64(src,&data))
         return -EFAULT;
      if((((data + 0x7ffefefefefefefful) ^ (~data)) & 0x7ffefefefefefefful) != 0)
         break;
      *(unsigned long *)dest = data;
   }

slow:
   for(int i = 0;i < n;++i)
      if(getUser8(src++,&data8))
         return -EFAULT;
      else if(!data8)
         break;
      else 
         *dest++ = data8;

done:
   *dest++ = 0;
   return dest - __dest; /*How much data did we copy?*/
}

long strncpyUser0(UserSpace(char) *dest,const char *src,unsigned long n)
{
   unsigned long align;
   unsigned long how;
   const char *__src = src;

   if(verifyUserAddress(dest,1))
      return -EFAULT; /*Verify the address.*/
   n = min(getCurrentTask()->addressLimit - (unsigned long)dest,n);
   --n;

   if((align = (((unsigned long)dest) & 7)) != (((unsigned long)src) & 7))
      goto slow;
   if(n < 8)
      goto slow;
   if(align)
      align = 8 - align;
   if(align && (n - align) < 8)
      goto slow;

   if(align)
      for(int i = 0;i < align;++align)
         if(*src == 0)
            goto done; /*No more data.*/
         else if(putUser8(dest,*src++))
            return -EFAULT; /*Bad address.*/

   how = n >> 3;
   for(int i = 0;i < how;++i,src += 8,dest += 8,n -= 8)
   {
      unsigned long data = *(unsigned long *)src;
      if((((data + 0x7ffefefefefefefful) ^ (~data)) & 0x7ffefefefefefefful) != 0)
         goto slow;
      if(putUser64(dest,data))
         return -EFAULT;
   }

slow:
   for(int i = 0;i < n;++i)
      if(!*src)
         break;
      else if(putUser8(dest++,*src++))
         return -EFAULT;

done:
   if(putUser8(dest++,0))
      return -EFAULT;
   return ++src - __src; /*Return the number of the data that we have copied. (Including '\0'.)*/
}
