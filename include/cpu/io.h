#pragma once

#include <core/const.h>

inline u8 inb(u16 port) __attribute__ ((always_inline));
inline int outb(u16 port,u8 data) __attribute__ ((always_inline));

inline int closeInterrupt() __attribute__ ((always_inline));
inline int startInterrupt() __attribute__ ((always_inline));
inline u64 storeInterrupt(void) __attribute__ ((always_inline));
inline int restoreInterrupt(u64 rflags) __attribute__ ((always_inline));

inline u8 inb(u16 port)
{
   u8 data;
   asm volatile("inb %%dx,%%al":"=a"(data):"d"(port));
   return data;
}

inline int outb(u16 port,u8 data)
{
   asm volatile("outb %%al,%%dx"::"a"(data),"d"(port));
   return 0;
}

inline int closeInterrupt()
{
   asm volatile("cli");
   return 0;
}

inline int startInterrupt()
{
   asm volatile("sti");
   return 0;
}
inline u64 storeInterrupt(void) 
{
   u64 ret;
   asm volatile(
      "pushfq\n\t"
      "popq %%rax"
      :"=a"(ret));
   return ret;

}
inline int restoreInterrupt(u64 rflags) 
{
   asm volatile(
      "pushq %%rax\n\t"
      "popfq"
      :
      :"a"(rflags));
   return 0;
}
