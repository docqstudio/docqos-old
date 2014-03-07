#include <unistd.h>
#include <errno.h>

int errno;

#define __NR_execve            0x0000
#define __NR_open              0x0001
#define __NR_read              0x0002
#define __NR_write             0x0003
#define __NR_close             0x0004
#define __NR_fork              0x0005
#define __NR_exit              0x0006
#define __NR_waitpid           0x0007
#define __NR_reboot            0x0008
#define __NR_getpid            0x0009
#define __NR_gettimeofday      0x000a
#define __NR_getdents64        0x000b
#define __NR_chdir             0x000c
#define __NR_getcwd            0x000d
#define __NR_lseek             0x000e
#define __NR_dup               0x000f
#define __NR_dup2              0x0010

#define __syscall0(ret,name)  \
   ret name(void) \
   { \
      unsigned long __ret; \
      asm volatile( \
         "int $0xff" \
         : "=a" (__ret) \
         : "a" (__NR_##name) \
      ); \
      if((long)__ret < 0 && (long)__ret >= -200) \
      { \
         errno = -__ret; \
         __ret = -1; \
      } \
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
      if((long)__ret < 0 && (long)__ret >= -200) \
      { \
         errno = -__ret; \
         __ret = -1; \
      } \
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
      if((long)__ret < 0 && (long)__ret >= -200) \
      { \
         errno = -__ret; \
         __ret = -1; \
      } \
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
      if((long)__ret < 0 && (long)__ret >= -200) \
      { \
         errno = -__ret; \
         __ret = -1; \
      } \
     return (ret)__ret; \
   }

__syscall0(int,fork);
__syscall0(int,getpid);

__syscall1(int,exit,int,code);
__syscall1(int,close,int,fd);
__syscall1(int,reboot,unsigned long,command);
__syscall1(int,chdir,const char *,dir);
__syscall1(int,dup,int,fd);

__syscall2(int,gettimeofday,unsigned long *,time,void *,unused);
__syscall2(int,dup2,int,fd,int,new);
__syscall2(int,getcwd,char *,buf,unsigned long,size);
__syscall2(int,open,const char *,path,int,mode);

__syscall3(int,execve,const char *,path,const char **,argc,const char **,envp);
__syscall3(unsigned long,read,int,fd,void *,buf,unsigned long,size);
__syscall3(unsigned long,getdents64,int,fd,void *,buf,unsigned long,size);
__syscall3(unsigned long,write,int,fd,const void *,buf,unsigned long,size);
__syscall3(int,waitpid,int,pid,int *,result,int,nowait);
__syscall3(unsigned long,lseek,int,fd,signed long,offset,int,type);
