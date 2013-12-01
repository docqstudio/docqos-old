#pragma once
#include <core/const.h>

typedef struct AtomicType{
   u32 data;
} AtomicType;

inline u32 atomicRead(AtomicType *atomic) __attribute__ ((always_inline));
inline int atomicSet(AtomicType *atomic,u32 data) __attribute__ ((always_inline));

inline int atomicAdd(AtomicType *atomic,u32 data) __attribute__ ((always_inline));
inline int atomicSub(AtomicType *atomic,u32 data) __attribute__ ((always_inline));

inline u32 atomicAddRet(AtomicType *atomic,u32 data) __attribute__ ((always_inline));

inline u32 atomicRead(AtomicType *atomic)
{
   return atomic->data;
}
inline int atomicSet(AtomicType *atomic,u32 data) 
{
   return (atomic->data = data,0);
}

inline int atomicAdd(AtomicType *atomic,u32 data) 
{
   asm volatile(
      "lock;addl %%ecx,(%%rax)"
      :
      :"a" (&atomic->data),"c" (data)
   );
   return 0;
}

inline int atomicSub(AtomicType *atomic,u32 data)
{
   u8 ret;
   asm volatile(
      "lock;subl %%ecx,(%%rax)\n\t"
      "sete %%bl"
      : "=b" (ret)
      : "a" (&atomic->data),"c" (data)
   );
   return !!(ret);
}
inline u32 atomicAddRet(AtomicType *atomic,u32 data)
{
   u32 __data;
   asm volatile(
      "lock;xaddl %%ecx,(%%rax)"
      : "=c"(__data)
      : "a" (&atomic->data),"c" (data)
   );
   return data + __data;
}
