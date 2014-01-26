#include <unistd.h>

#define __NR_execve 0x0
#define __NR_open 0x1
#define __NR_read 0x2
#define __NR_write 0x3
#define __NR_close 0x4
#define __NR_fork 0x5
#define __NR_exit  0x6
#define __NR_waitpid 0x7
#define __NR_reboot 0x8
#define __NR_getpid 0x9
#define __NR_gettimeofday 0xa
#define __NR_getdents64 0xb

#define __syscall0(ret,name)  \
   ret name(void) \
   { \
      unsigned long __ret; \
      asm volatile( \
         "int $0xff" \
         : "=a" (__ret) \
         : "a" (__NR_##name) \
      ); \
      return (ret)__ret; \
   }

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

#define __syscall2(ret,name,a1,__a1,a2,__a2)  \
   ret name(a1 __a1,a2 __a2) \
   { \
      unsigned long __ret; \
      asm volatile( \
         "int $0xff" \
         : "=a" (__ret) \
         : "a" (__NR_##name),"b" ((unsigned long)__a1), \
           "c" ((unsigned long)__a2) \
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

__syscall0(int,fork);
__syscall0(int,getpid);
__syscall1(int,exit,int,code);
__syscall1(int,open,const char *,filename);
__syscall1(int,close,int,fd);
__syscall1(int,reboot,unsigned long,command);
__syscall2(int,gettimeofday,unsigned long *,time,void *,unused);
__syscall3(int,execve,const char *,path,const char **,argc,const char **,envp);
__syscall3(unsigned long,read,int,fd,void *,buf,unsigned long,size);
__syscall3(unsigned long,getdents64,int,fd,void *,buf,unsigned long,size);
__syscall3(unsigned long,write,int,fd,const void *,buf,unsigned long,size);
__syscall3(int,waitpid,int,pid,int *,result,int,nowait);
