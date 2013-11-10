#include <video/vesa.h>
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
   writeStringInColor(red,green,blue,buf);
   return ret;
}
/*It returns how many chars it displayed.*/
int printk(const char *string, ...)
{
   char buf[256];
   int ret = 0;
   VarArgsList list;
   varArgsStart(list,string);
   ret = (int)(vsprintk(buf,string,list) - buf);
   varArgsEnd(list);
   writeString(buf);
   return ret;
}
char *vsprintk(char *buf,const char *string,VarArgsList list) 
/*Now it only supports %d,%x,%s.*/
{
   char temp[256];
   char c;
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
         {
            int val = varArgsNext(list,int);
	    *(buf++) = '0';
	    *(buf++) = 'x'; 
            int len = (int)(itoa(val,temp,16,0,0,1) - temp);/*It's unsigned.*/
	    /*This function itoa returns the end address.*/
	    memcpy((void *)buf,(const void *)temp,len);
	    buf += len;
            break;
	}
      case 'd':
         {
            int val = varArgsNext(list,int);
	    int len = (int)(itoa(val,temp,10,0,0,0) - temp);
	    memcpy((void *)buf,(const void *)temp,len);
	    buf += len;
	    break;
	 }
      case 's':
         {
	    const char *s = varArgsNext(list,const char *);
	    int len = strlen(s);
	    memcpy((void *)buf,(const void *)s,len);
	    buf += len;
            break;
	 } 
      default:
         break;
      }
   }
   *buf = '\0';
   return buf;
}
