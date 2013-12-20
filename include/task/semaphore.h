#pragma once
#include <core/const.h>
#include <task/waitqueue.h>
#include <cpu/atomic.h>

typedef struct Semaphore{
   AtomicType count;
   WaitQueue queue;
   int sleepers;
} Semaphore;

inline int initSemaphore(Semaphore *sem)
   __attribute__ ((always_inline));

int downSemaphore(Semaphore *sem);
int upSemaphore(Semaphore *sem);

inline int initSemaphore(Semaphore *sem)
{
   sem->sleepers = 0;
   initWaitQueueHead(&sem->queue);
   atomicSet(&sem->count,1);
   return 0;
}
