#include <core/const.h>
#include <video/console.h>

/*int handleException(u64 rdi,u64 rsi,u64 rbp,
   u64 rdx,u64 rcx,u64 rbx,u64 rax,u64 index,
   u64 errorCode,u64 rip,u64 cs,u64 rflags,
   u64 rsp,u64 ss) __attribute__((regparm(3)));*/
int handleException(void)
{
   printkInColor(0xff,0x00,0x00,"\n\nException!!!!!!!\n");
   for(;;);
   return 0;
}
