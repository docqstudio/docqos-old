#pragma once

#include <core/const.h>

inline u8 inb(u16 port) __attribute__ ((always_inline));
inline int outb(u16 port,u8 data) __attribute__ ((always_inline));
inline u16 inw(u16 port) __attribute__ ((always_inline));
inline int outw(u16 port,u16 data) __attribute__ ((always_inline));
inline u32 inl(u16 port) __attribute__ ((always_inline));
inline int outl(u16 port,u32 data) __attribute__ ((always_inline));

inline int insl(u16 port,u64 size,void *to) __attribute__ ((always_inline));
inline int insw(u16 port,u64 size,void *to) __attribute__ ((always_inline));
inline int outsw(u16 port,u64 size,void *from) __attribute__ ((always_inline));

inline int closeInterrupt(void) __attribute__ ((always_inline));
inline int startInterrupt(void) __attribute__ ((always_inline));
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
inline u16 inw(u16 port)
{
   u16 data;
   asm volatile("inw %%dx,%%ax":"=a"(data):"d"(port));
   return data;
}
inline int outw(u16 port,u16 data)
{
   asm volatile("outw %%ax,%%dx"::"a"(data),"d"(port));
   return 0;
}

inline u32 inl(u16 port)
{
   u32 data;
   asm volatile("inl %%dx,%%eax":"=a"(data):"d"(port));
   return data;
}

inline int outl(u16 port,u32 data)
{
   asm volatile("outl %%eax,%%dx"::"a"(data),"d"(port));
   return 0;
}

inline int insl(u16 port,u64 size,void *to) 
{
   asm volatile(
      "rep;insl"
      :"=D"(to),"=c"(size)
      :"D"(to),"c"(size),"d"((u64)port)
      :"memory"
   );
   return 0;
}

inline int insw(u16 port,u64 size,void *to) 
{
   asm volatile(
      "rep;insw"
      :"=D"(to),"=c"(size)
      :"D"(to),"c"(size),"d"((u64)port)
      :"memory"
   );
   return 0;
}

inline int outsw(u16 port,u64 size,void *from) 
{
   asm volatile(
      "rep;outsw"
      :"=S"(from),"=c"(size)
      :"S"(from),"c"(size),"d"((u64)port)
      :"memory"
   );
   return 0;
}
inline int closeInterrupt(void)
{
   asm volatile("cli");
   return 0;
}

inline int startInterrupt(void)
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
