#pragma once
#include <core/const.h>

typedef struct AtomicType{
   int data;
} AtomicType;

inline int atomicRead(AtomicType *atomic) __attribute__ ((always_inline));
inline int atomicSet(AtomicType *atomic,int data) __attribute__ ((always_inline));

inline int atomicAdd(AtomicType *atomic,int data) __attribute__ ((always_inline));
inline int atomicSub(AtomicType *atomic,int data) __attribute__ ((always_inline));

inline int atomicAddRet(AtomicType *atomic,int data) __attribute__ ((always_inline));
inline int atomicSubRet(AtomicType *atomic,int data) __attribute__ ((always_inline));

inline int atomicCompareExchange(AtomicType *atomic,int old,int new)
                              __attribute__ ((always_inline));

inline int atomicRead(AtomicType *atomic)
{
   return atomic->data;
}
inline int atomicSet(AtomicType *atomic,int data) 
{
   return (atomic->data = data,0);
}

inline int atomicAdd(AtomicType *atomic,int data) 
{
   asm volatile(
      "lock;addl %%ecx,(%%rax)"
      :
      :"a" (&atomic->data),"c" (data)
   );
   return 0;
}

inline int atomicSub(AtomicType *atomic,int data)
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

inline int atomicAddRet(AtomicType *atomic,int data)
{
   int __data;
   asm volatile(
      "lock;xaddl %%ecx,(%%rax)"
      : "=c"(__data)
      : "a" (&atomic->data),"c" (data)
   );
   return data + __data;
}

inline int atomicSubRet(AtomicType *atomic,int data)
{
   return atomicAddRet(atomic,-data);
}

inline int atomicCompareExchange(AtomicType *atomic,int old,int new)
{
   int retval;
   asm volatile(
      "lock;cmpxchg %%ecx,(%%rbx)"
      : "=a" (retval)
      : "a" (old),"c" (new), "b"(&atomic->data)
   );
   return retval;
}
