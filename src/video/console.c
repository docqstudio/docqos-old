#include <core/const.h>
#include <video/console.h>
#include <video/framebuffer.h>
#include <driver/serialport.h>
#include <lib/stdarg.h>
#include <lib/string.h>

char *vsprintk(char *buf,const char *string,VarArgsList list);

int printkInColor(u8 red,u8 green,u8 blue,const char *string, ...)
{
   char buf[256];
   int ret = 0;
   VarArgsList list;
   varArgsStart(list,string);
   ret = (int)(vsprintk(buf,string,list) - buf);
   varArgsEnd(list);
   
   frameBufferWriteStringInColor(red,green,blue,buf,0,1);
#if defined(CONFIG_DEBUG)
   writeSerialPort(buf,0);
      /*Write to the screen and the first serial port.*/
#endif
   return ret;
}

int printk(const char *string, ...)
{
   char buf[256];
   int ret = 0;
   VarArgsList list;
   varArgsStart(list,string);
   ret = (int)(vsprintk(buf,string,list) - buf);
   varArgsEnd(list);

   frameBufferWriteString(buf);
#if defined(CONFIG_DEBUG)
   writeSerialPort(buf,0);
#endif
   return ret;
}

#if defined(CONFIG_DEBUG)
int printl(const char *string,...)
{
   char buf[256];
   int ret = 0;
   VarArgsList list;
   varArgsStart(list,string);
   ret = (int)(vsprintk(buf,string,list) - buf);
   varArgsEnd(list);

   writeSerialPort(buf,0); /*Write to the first serial port.*/
   return ret;
}
#endif 

char *vsprintk(char *buf,const char *string,VarArgsList list) 
/*Now it only supports %d,%x,%s.*/
{
   char temp[256];
   const char *number;
   char c,longlong = 0,align = 0,with = ' ';
   char isunsigned = 0;
   while((c = *string++) != 0)
   {
      if(c != '%')
      {
         *(buf++) = c;
         continue;
      }
      c = *string++;
      if(c == '0')
         with = '0'; /*Align with '0'.*/
      else if(--string)       /*Always be true.*/
         with = ' '; /*Align with ' '.*/

      if(*string == '*')
      {
         align = varArgsNext(list,unsigned int),++string;
         goto next;
      }
      number = string;
      while((c = (*string++)) && (c >= '0') && (c <= '9'));
         ;
      string -= 2;
      for(int i = 0,j = 1;&string[i] != &number[-1];--i,j *= 10)
         align += (string[i] - '0') * j;
      ++string;

next:
      while((c = *string++) == 'l')
         ++longlong;
      if(longlong > 2)
         continue;
      switch(c)
      {
      case '%':
         *buf++ = '%';
         break;
      case 'p':
         longlong = 1;
      case 'x':
         {
            unsigned long long val = 0;
            if(longlong == 0)
               val = varArgsNext(list,unsigned int);
            else if(longlong == 1)
               val = varArgsNext(list,unsigned long);
            else
               val = varArgsNext(list,unsigned long long);
            int len = (int)(itoa(val,temp,16,align,with,1) - temp);/*It's unsigned.*/
            /*This function itoa returns the end address.*/
            memcpy((void *)buf,(const void *)temp,len);
            buf += len;
            break;
        }
      case 'u': /*Unsigned format.*/
         isunsigned = 1;
      case 'd':
         {
            unsigned long long val; /*It must be unsigned!!!!*/
            if(longlong == 0)
               val = varArgsNext(list,unsigned int);
            else if(longlong == 1)
               val = varArgsNext(list,unsigned long);
            else
               val = varArgsNext(list,unsigned long long);
            int len = (int)(itoa(val,temp,10,align,with,isunsigned) - temp);
            memcpy((void *)buf,(const void *)temp,len);
            buf += len;
            break;
         }
      case 's':
         {
            const char *s = varArgsNext(list,const char *);
            int len = align ? : strlen(s);
            memcpy((void *)buf,(const void *)s,len);
            buf += len;
            break;
         }
      case 'c':
         *(buf++) = varArgsNext(list,int);
         break;
      default:
         break;
      }
      longlong = 0;
      align = 0; /*Restore these values.*/
      isunsigned = 0;
   }
   *buf = '\0';
   (void)with;
   return buf;
}
