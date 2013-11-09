#include <video/vesa.h>
#include <lib/stdarg.h>
#include <lib/string.h>

int vsprintk(char *buf,const char *string,VarArgsList list);

int printk(const char *string, ...)
{
   char buf[256];
   VarArgsList list;
   varArgsStart(list,string);

   vsprintk(buf,string,list);

   varArgsEnd(list);

   writeString(buf);

   return 0;
}

int vsprintk(char *buf,const char *string,VarArgsList list)
{
   char temp[256];
   char c;
   int val,len;
   while((c = *(string++)) != 0)
   {
      if(c != '%')
      {
         *(buf++) = c;
	 continue;
      }
      c = *(string++);
      switch(c)
      {
      case '%':
         *(buf++) = '%';
	 break;
      case 'x':
         val = varArgsNext(list,int);
	 if(val < 0)
	 {
	    *(buf++) = '-';
	    val = -val;
	 }
	 *(buf++) = '0';
	 *(buf++) = 'x'; 
         len = (int)(itoa(val,temp,16) - temp);
	 /*This function itoa returns the end address.*/
	 memcpy((void *)buf,(const void *)temp,len);
	 buf += len;
         break;
      case 'd':
         val = varArgsNext(list,int);
	 len = (int)(itoa(val,temp,10) - temp);
	 memcpy((void *)buf,(const void *)temp,len);
	 buf += len;
	 break;
      default:
         break;
      }
   }
   return 0;
}
