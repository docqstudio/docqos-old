#pragma once
#include <core/const.h>
#include <cpu/atomic.h>
#include <cpu/spinlock.h>

typedef int (RCUCallback)(void *data);

typedef struct RCULock
{
   RCUCallback *callbacks[20];
   void *data[20];
   AtomicType count;
   int ccount;
   SpinLock lock;
} RCULock;

inline int initRCULock(RCULock *lock) __attribute__((always_inline));

inline int initRCULock(RCULock *rcu)
{
   initSpinLock(&rcu->lock);
   atomicSet(&rcu->count,0);
   rcu->ccount = 0;
   return 0;
}

int lockRCUReadLock(RCULock *lock);
int unlockRCUReadLock(RCULock *lock);

int addRCUCallback(RCULock *lock,RCUCallback *callback,void *data);
