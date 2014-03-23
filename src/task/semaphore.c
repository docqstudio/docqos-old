#include <core/const.h>
#include <task/semaphore.h>
#include <video/console.h>
#include <cpu/io.h>

int downSemaphore(Semaphore *sem)
{
   Task *current = getCurrentTask();
   if(!current)
      return 0; /*Nothing to do.*/
   {
      if(likely(atomicAddRet(&sem->count,-1) >= 0))
         return 0; /*If unlock,just lock it and return.*/
      WaitQueue wait;
      initWaitQueue(&wait,current);

      lockSpinLock(&sem->queue.lock); /*Lock this semaphore's spin lock.*/
      addToWaitQueueLocked(&wait,&sem->queue);
             /*Add this task to WaitQueue.*/
      ++sem->sleepers;
      for(;;)
      {
         if(atomicAddRet(&sem->count,sem->sleepers - 1) >= 0)
         {
            /*Down this semaphore successfully,return.*/
            sem->sleepers = 0;
            break;
         }
         sem->sleepers = 1;
         current->state = TaskStopping;
         unlockSpinLock(&sem->queue.lock);
         schedule(); /*Wait.*/
         lockSpinLock(&sem->queue.lock);
         current->state = TaskRunning;
      }
      removeFromWaitQueueLocked(&wait); /*Remove this task.*/
      wakeUpLocked(&sem->queue); /*Wake up one sleeping task.*/
      unlockSpinLock(&sem->queue.lock); /*Unlock and return.*/
   }
   return 0;
}

int upSemaphore(Semaphore *sem)
{
   if(!getCurrentTask())
      return 0; /*Nothing to do.*/
   if(atomicAddRet(&sem->count,1) >= 0)
      wakeUp(&sem->queue); /*Wake up one sleeping task if we need.*/
   return 0;
}
