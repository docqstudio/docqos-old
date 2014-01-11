
#define __NR_open 0x1
#define __NR_read 0x2
#define __NR_write 0x3
#define __NR_close 0x4

#define __syscall1(ret,name,a1,__a1)  \
   ret name(a1 __a1) \
   { \
      unsigned long __ret; \
      asm volatile( \
         "int $0xff" \
         : "=a" (__ret) \
         : "a" (__NR_##name),"b" ((unsigned long)__a1) \
      ); \
      return (ret)__ret; \
   }

#define __syscall3(ret,name,a1,__a1,a2,__a2,a3,__a3)  \
   ret name(a1 __a1,a2 __a2,a3 __a3) \
   { \
      unsigned long __ret; \
      asm volatile( \
         "int $0xff" \
         : "=a" (__ret) \
         : "a" (__NR_##name),"b" ((unsigned long)__a1), \
           "c" ((unsigned long)__a2),"d" ((unsigned long)__a3) \
      ); \
      return (ret)__ret; \
   }

__syscall1(int,open,const char *,filename);
__syscall1(int,close,int,fd);
__syscall3(int,read,int,fd,void *,buf,unsigned long,size);
__syscall3(int,write,int,fd,const void *,buf,unsigned long,size);

int main(void)
{
   volatile char name[20] = {};
   int fd = open("/dev/tty");
   write(fd,"Welcome to DOCQ OS,this is a shabby shell!!\n",0);
   for(;;)
   {
      write(fd,"\xffs\xff\x00\x00localhost \xffs\x00\x00\xff/ $ ",0);
      read(fd,(void *)name,19);
      if(name[0] == '\0')
         continue;
      if(name[0] == 'e' &&
         name[1] == 'x' &&
         name[2] == 'i' &&
         name[3] == 't' &&
         name[4] == '\0')
      {
         write(fd,"Good bye!\n",0);
         break;
      }
      write(fd,(const void *)name,0);
      write(fd,"\n",0);
   }
   close(fd);
   return 0;
}
