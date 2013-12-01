#pragma once
#include <core/const.h>
#include <cpu/spinlock_types.h>
#include <task/task.h>

#define lockSpinLockDisableInterrupt(lock,rflags) \
  do{ \
     *(rflags) = storeInterrupt(); \
     closeInterrupt();\
     lockSpinLock(lock); \
  }while(0)

#define unlockSpinLockRestoreInterrupt(lock,rflags) \
   do{ \
      unlockSpinLock(lock); \
      restoreInterrupt(*(rflags)); \
   }while(0)

#define unlockSpinLockEnableInterrupt(lock) \
   do{ \
      unlockSpinLock(lock); \
      startInterrupt(); \
   }while(0)

inline int initSpinLock(SpinLock *lock) __attribute__ ((always_inline));
inline int lockSpinLock(SpinLock *lock) __attribute__ ((always_inline));
inline int unlockSpinLock(SpinLock *lock) __attribute__ ((always_inline));

inline int initSpinLock(SpinLock *lock)
{
   return !((lock->lock = 1));
}

inline int lockSpinLock(SpinLock *lock)
{
   disablePreemption();
   asm volatile(
      "xorq %%rbx,%%rbx\n\t"
      "2:\n\t" /*label retry*/
      "xchgb %%bl,(%%rax)\n\t"
      "cmpb $1,%%bl\n\t"
      "je 1f\n\t"
      "3:\n\t" /*label loop*/
      "pause;pause;pause;pause\n\t"
      "cmpb $1,(%%rax)\n\t"
      "je 2b\n\t"
      "jmp 3b\n\t"
      "1:" /*label end*/
      :
      :"a"(&lock->lock)
      /*&lock->lock -> %rax */
      :"%rbx"
   );
   return 0;
}

inline int unlockSpinLock(SpinLock *lock)
{
   lock->lock = 1;
   enablePreemption();
   return 0;
}
