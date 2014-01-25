#pragma once
#include <core/const.h>
#include <core/list.h>
#include <task/task.h>
#include <cpu/spinlock.h>

typedef struct WaitQueue{
   Task *task;
   ListHead list;
   SpinLock lock; /*This field is only used in a WaitQueue head.*/
} WaitQueue;

inline int initWaitQueueHead(WaitQueue *queue)
   __attribute__ ((always_inline));
inline int initWaitQueue(WaitQueue *queue,Task *task) 
   __attribute__ ((always_inline));
inline int addToWaitQueueLocked(WaitQueue *wait,WaitQueue *head)
   __attribute__ ((always_inline));
inline int wakeUpLocked(WaitQueue *queue)
   __attribute__ ((always_inline));
inline int wakeUp(WaitQueue *queue)
   __attribute__ ((always_inline));
inline int addToWaitQueue(WaitQueue *wait,WaitQueue *head)
   __attribute__ ((always_inline));
inline int removeFromWaitQueueLocked(WaitQueue *wait)
   __attribute__ ((always_inline));
inline int removeFromWaitQueue(WaitQueue *head,WaitQueue *wait)
   __attribute__ ((always_inline));

inline int initWaitQueueHead(WaitQueue *queue)
{
   initList(&queue->list);
   initSpinLock(&queue->lock);
   queue->task = 0;
   return 0;
}

inline int initWaitQueue(WaitQueue *queue,Task *task)
{
   initList(&queue->list);
   queue->task = task;
      /*SpinLock is not used.*/
   return 0;
}

inline int addToWaitQueueLocked(WaitQueue *wait,WaitQueue *head)
{
   return listAdd(&wait->list,&head->list);
}

inline int wakeUpLocked(WaitQueue *queue)
{
   if(listEmpty(&queue->list))
      return 0; /*Just do nothing if this list is empty.*/
   WaitQueue *wait = listEntry(queue->list.next,WaitQueue,list);
   Task *task = wait->task; /*Get the task.*/
   return wakeUpTask(task); /*Wake up it!*/
}

inline int removeFromWaitQueueLocked(WaitQueue *wait)
{
   return listDelete(&wait->list);
}

/*These functions below do these things:*/
/*1.lock a spin lock.*/
/*2.call *Locked.*/
/*3.unlock the spin lock.*/
inline int addToWaitQueue(WaitQueue *wait,WaitQueue *head)
{
   lockSpinLock(&head->lock);
   addToWaitQueueLocked(wait,head);
   unlockSpinLock(&head->lock);
   return 0;
}

inline int wakeUp(WaitQueue *queue)
{
   lockSpinLock(&queue->lock);
   wakeUpLocked(queue);
   unlockSpinLock(&queue->lock);
   return 0;
}

inline int removeFromWaitQueue(WaitQueue *head,WaitQueue *wait)
{
   lockSpinLock(&head->lock);
   removeFromWaitQueueLocked(wait);
   unlockSpinLock(&head->lock);
   return 0;
}
